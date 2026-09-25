# src/ph7/builtin_parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2561/2770 lines (92.45%)

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
|     - |   42 | `#define FV_SANITIZE_STRING             513` |
|     - |   43 | `#define FV_SANITIZE_ENCODED            514` |
|     - |   44 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|     - |   45 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|     - |   46 | `#define FV_SANITIZE_EMAIL   517` |
|     - |   47 | `#define FV_SANITIZE_URL     518` |
|     - |   48 | `#define FV_SANITIZE_NUMBER_INT   519` |
|     - |   49 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|     - |   50 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|     - |   51 | `#define FV_SANITIZE_ADD_SLASHES        523` |
|     - |   52 | `#define FV_CALLBACK                    1024` |
|     - |   53 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|     - |   54 | `#define FV_FLAG_ALLOW_HEX    2` |
|     - |   55 | `#define FV_FLAG_STRIP_LOW    4` |
|     - |   56 | `#define FV_FLAG_STRIP_HIGH   8` |
|     - |   57 | `#define FV_FLAG_ENCODE_LOW   16` |
|     - |   58 | `#define FV_FLAG_ENCODE_HIGH  32` |
|     - |   59 | `#define FV_FLAG_ENCODE_AMP   64` |
|     - |   60 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|     - |   61 | `#define FV_FLAG_EMPTY_STRING_NULL 256` |
|     - |   62 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|     - |   63 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|     - |   64 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|     - |   65 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|     - |   66 | `#define FV_FLAG_PATH_REQUIRED  262144` |
|     - |   67 | `#define FV_FLAG_QUERY_REQUIRED 524288` |
|     - |   68 | `#define FV_FLAG_IPV4  1048576` |
|     - |   69 | `/* php gives one bit three names, one per filter it belongs to. */` |
|     - |   70 | `#define FV_FLAG_HOSTNAME      1048576` |
|     - |   71 | `#define FV_FLAG_EMAIL_UNICODE 1048576` |
|     - |   72 | `#define FV_FLAG_IPV6  2097152` |
|     - |   73 | `#define FV_FLAG_NO_RES_RANGE  4194304` |
|     - |   74 | `#define FV_FLAG_NO_PRIV_RANGE 8388608` |
|     - |   75 | `#define FV_FLAG_GLOBAL_RANGE  536870912` |
|     - |   76 | `#define FV_REQUIRE_ARRAY   16777216` |
|     - |   77 | `#define FV_REQUIRE_SCALAR  33554432` |
|     - |   78 | `#define FV_FORCE_ARRAY     67108864` |
|     - |   79 | `#define FV_NULL_ON_FAILURE 134217728` |
|     - |   80 | `#define FV_THROW_ON_FAILURE 268435456` |
|     - |   81 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|     - |   82 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|     - |   83 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|     - |   84 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|     - |   85 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|     - |   86 |  |
|     - |   87 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|     - |   88 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|   323 |   89 | `static void FvTrim(const char **pz,int *pn){` |
|   323 |   90 | `	const char *z = *pz;` |
|   323 |   91 | `	int n = *pn;` |
|   327 |   92 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|   331 |   93 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|   323 |   94 | `	*pz = z; *pn = n;` |
|   323 |   95 | `}` |
|     - |   96 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|   161 |   97 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|   161 |   98 | `	int neg = 0, i;` |
|   161 |   99 | `	sxu64 u = 0;` |
|   161 |  100 | `	FvTrim(&z,&n);` |
|   161 |  101 | `	if( n==0 ){ return 0; }` |
|   149 |  102 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|   149 |  103 | `	if( n==0 ){ return 0; }` |
|   147 |  104 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|     3 |  105 | `		z += 2; n -= 2;` |
|     3 |  106 | `		if( n==0 ){ return 0; }` |
|     7 |  107 | `		for( i=0; i<n; i++ ){` |
|     5 |  108 | `			int h = SyHexToint((unsigned char)z[i]);` |
|     5 |  109 | `			if( h<0 ){ return 0; }` |
|     5 |  110 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|     5 |  111 | `			u = u*16 + (sxu64)h;` |
|     3 |  112 | `		}` |
|   144 |  113 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|     9 |  114 | `		for( i=0; i<n; i++ ){` |
|     7 |  115 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|     7 |  116 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|     7 |  117 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|     4 |  118 | `		}` |
|     2 |  119 | `	}else{` |
|   143 |  120 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|   381 |  121 | `		for( i=0; i<n; i++ ){` |
|   283 |  122 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|   245 |  123 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|   245 |  124 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|   124 |  125 | `		}` |
|     - |  126 | `	}` |
|   105 |  127 | `	if( neg ){` |
|     5 |  128 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|     5 |  129 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|     3 |  130 | `	}else{` |
|   101 |  131 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|    99 |  132 | `		*pOut = (ph7_int64)u;` |
|     - |  133 | `	}` |
|   103 |  134 | `	return 1;` |
|    82 |  135 | `}` |
|     - |  136 | `/* Is byte c one of the nSep thousand separators in zSep? */` |
|    52 |  137 | `static int FvIsThousandSep(const char *zSep,int nSep,int c){` |
|     - |  138 | `	int i;` |
|   102 |  139 | `	for( i=0; i<nSep; i++ ){` |
|    96 |  140 | `		if( (unsigned char)zSep[i] == (unsigned char)c ){ return 1; }` |
|    27 |  141 | `	}` |
|     8 |  142 | `	return 0;` |
|    27 |  143 | `}` |
|     - |  144 | `/*` |
|     - |  145 | ` * FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure.` |
|     - |  146 | ` *` |
|     - |  147 | ` * decSep is the byte that separates the fractional part (php's "decimal" option,` |
|     - |  148 | ` * '.' by default) and zSep[0..nSep) the set that may group the INTEGER part when` |
|     - |  149 | ` * FILTER_FLAG_ALLOW_THOUSAND is set (php's "thousand" option, "',." by default).` |
|     - |  150 | ` * The two sets overlap by default, and php tests the decimal separator FIRST —` |
|     - |  151 | `` * which is why `decimal => ','` leaves '.' working as a group separator.`` |
|     - |  152 | ` *` |
|     - |  153 | ` * The number is normalized into zBuf as a plain C double literal (separators` |
|     - |  154 | ` * dropped, decSep rewritten to '.') and handed to strtod.` |
|     - |  155 | ` */` |
|   128 |  156 | `static int FvValidateFloat(const char *z,int n,int flags,int decSep,` |
|     2 |  157 | `                           const char *zSep,int nSep,double *pOut){` |
|     - |  158 | `	/* decSep is a BYTE value (0..255): a separator above 127 — php takes any` |
|     - |  159 | `	 * single byte, including one out of a UTF-8 sequence — must not be compared` |
|     - |  160 | `	 * against a sign-extended char. */` |
|     - |  161 | `	char zBuf[512];` |
|   130 |  162 | `	int i = 0, m = 0, seenDigit = 0, grouped = 0, nGroup = 0, runLen;` |
|   130 |  163 | `	int hasExp = 0, expNonZero = 0, hasDot = 0;` |
|   130 |  164 | `	double d = 0;` |
|   130 |  165 | `	FvTrim(&z,&n);` |
|     - |  166 | `	/* Bound the input: zBuf[512] holds the separator-stripped copy, and the cap` |
|     - |  167 | `	 * also rejects the pathological 500+ digit floats PHP refuses. */` |
|   130 |  168 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|   130 |  169 | `	if( i<n && (z[i]=='+'\|\|z[i]=='-') ){ zBuf[m++] = z[i]; i++; }` |
|     - |  170 | `	/* The integer part: digit runs, optionally separated by a thousand separator.` |
|     - |  171 | `	 * A separator anywhere means the runs must GROUP — a leading run of 1..3` |
|     - |  172 | `	 * digits then runs of exactly 3 ("1,000" and "1'234,567" ok, "1,5" and` |
|     - |  173 | `	 * "1234,567" rejected); with no separator the run is any length at all` |
|     - |  174 | `	 * (including zero, which is how ".5" parses). */` |
|    64 |  175 | `	for(;;){` |
|   168 |  176 | `		runLen = 0;` |
|   410 |  177 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){ zBuf[m++] = z[i]; i++; runLen++; }` |
|   168 |  178 | `		if( runLen>0 ){ seenDigit = 1; }` |
|   166 |  179 | `		if( i<n && (unsigned char)z[i]!=decSep && (flags & FV_FLAG_ALLOW_THOUSAND)` |
|    70 |  180 | `		 && FvIsThousandSep(zSep,nSep,z[i]) ){` |
|    46 |  181 | `			if( nGroup==0 ){ if( runLen<1 \|\| runLen>3 ){ return 0; } }` |
|     8 |  182 | `			else if( runLen!=3 ){ return 0; }` |
|    40 |  183 | `			grouped = 1; nGroup++;` |
|    40 |  184 | `			i++;                       /* drop the separator itself */` |
|    40 |  185 | `			continue;` |
|     - |  186 | `		}` |
|   124 |  187 | `		if( grouped && runLen!=3 ){ return 0; } /* the run that closes a grouped number */` |
|   116 |  188 | `		break;` |
|   ! 0 |  189 | `	}` |
|   116 |  190 | `	if( i<n && (unsigned char)z[i]==decSep ){` |
|    52 |  191 | `		zBuf[m++] = '.';` |
|    52 |  192 | `		hasDot = 1;` |
|    52 |  193 | `		i++;` |
|    98 |  194 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){ zBuf[m++] = z[i]; i++; seenDigit = 1; }` |
|    25 |  195 | `	}` |
|   116 |  196 | `	if( !seenDigit ){ return 0; }` |
|   112 |  197 | `	if( i<n && (z[i]=='e'\|\|z[i]=='E') ){` |
|    38 |  198 | `		zBuf[m++] = z[i];` |
|    38 |  199 | `		i++;` |
|    38 |  200 | `		if( i<n && (z[i]=='+'\|\|z[i]=='-') ){ zBuf[m++] = z[i]; i++; }` |
|    38 |  201 | `		if( i>=n \|\| !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|   122 |  202 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|    86 |  203 | `			if( z[i]!='0' ){ expNonZero = 1; }` |
|    86 |  204 | `			zBuf[m++] = z[i]; i++;` |
|     2 |  205 | `		}` |
|    38 |  206 | `		hasExp = 1;` |
|    18 |  207 | `	}` |
|   112 |  208 | `	if( i!=n ){ return 0; } /* trailing junk */` |
|     - |  209 | `	/* The grammar above guarantees zBuf[0..m) is a clean ASCII decimal float (no hex /` |
|     - |  210 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|     - |  211 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|     - |  212 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|     - |  213 | `	 * correctly rounded. Every byte written to zBuf consumed one input byte, so` |
|     - |  214 | `	 * m <= n <= 500 < sizeof(zBuf) and the NUL below is in range.` |
|     - |  215 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|     - |  216 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|   100 |  217 | `	zBuf[m] = 0;` |
|   100 |  218 | `	errno = 0;` |
|   100 |  219 | `	d = strtod(zBuf,0);` |
|   100 |  220 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|    15 |  221 | `		return 0;` |
|     - |  222 | `	}` |
|     - |  223 | `	/* php's own strtod reports a zero answer carrying a non-zero EXPONENT as an` |
|     - |  224 | `	 * underflow, where glibc's leaves errno alone: "0e1", "0.e5" and ".0e5" are` |
|     - |  225 | `	 * refused while "0", "0.0" and "0e0" are the float zero. */` |
|    86 |  226 | `	if( d == 0.0 && hasExp && expNonZero ){ return 0; }` |
|     - |  227 | `	/* An INTEGER-shaped literal is answered through php's long path, which has no` |
|     - |  228 | `	 * signed zero: "-0" is the float +0.0 where "-0.0" and "-0e0" stay negative. */` |
|    82 |  229 | `	if( d == 0.0 && !hasDot && !hasExp ){ d = 0.0; }` |
|    82 |  230 | `	*pOut = d;` |
|    82 |  231 | `	return 1;` |
|    66 |  232 | `}` |
|     - |  233 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|     - |  234 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|     - |  235 | ` * false, NOT failures. */` |
|    43 |  236 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|    43 |  237 | `	FvTrim(&z,&n);` |
|    40 |  238 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|    35 |  239 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|    11 |  240 | `		*pBool = 1; return 1;` |
|     - |  241 | `	}` |
|    30 |  242 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|    17 |  243 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|    17 |  244 | `		*pBool = 0; return 1;` |
|     - |  245 | `	}` |
|    12 |  246 | `	return 0;` |
|    20 |  247 | `}` |
|     - |  248 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. The four octets` |
|     - |  249 | ` * land in aOut[] (which the range flags below read); pass 0 to only validate. */` |
|   593 |  250 | `static int FvValidateIp4(const char *z,int n,unsigned char *aOut){` |
|   593 |  251 | `	int i = 0, parts = 0;` |
|  2013 |  252 | `	while( i<n ){` |
|  1657 |  253 | `		int val = 0, digits = 0, start = i;` |
|  4523 |  254 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|  2944 |  255 | `			val = val*10 + (z[i]-'0');` |
|  2944 |  256 | `			if( val>255 ){ return 0; }` |
|  2868 |  257 | `			digits++; i++;` |
|     2 |  258 | `		}` |
|  1581 |  259 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|  1456 |  260 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|  1452 |  261 | `		if( parts>3 ){ return 0; }` |
|  1452 |  262 | `		if( aOut ){ aOut[parts] = (unsigned char)val; }` |
|  1452 |  263 | `		parts++;` |
|  1452 |  264 | `		if( i<n ){` |
|  1096 |  265 | `			if( z[i]!='.' ){ return 0; }` |
|  1066 |  266 | `			i++;` |
|  1066 |  267 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|   532 |  268 | `		}` |
|     2 |  269 | `	}` |
|   358 |  270 | `	return parts==4;` |
|   298 |  271 | `}` |
|     - |  272 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|     - |  273 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1, and` |
|     - |  274 | ` * writes the bytes it read at aOut (which must have room for 16). */` |
|   394 |  275 | `static int FvIp6Hextets(const char *z,int n,unsigned char *aOut){` |
|   394 |  276 | `	int i = 0, segStart = 0, groups = 0;` |
|   394 |  277 | `	if( n==0 ){ return 0; }` |
|  2268 |  278 | `	while( i<=n ){` |
|  1886 |  279 | `		if( i==n \|\| z[i]==':' ){` |
|   508 |  280 | `			int segLen = i - segStart, j, isV4 = 0;` |
|   508 |  281 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|  1698 |  282 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|   508 |  283 | `			if( isV4 ){` |
|    34 |  284 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|    34 |  285 | `				if( groups>6 ){ return -1; }` |
|    34 |  286 | `				if( !FvValidateIp4(z+segStart,segLen,aOut ? &aOut[groups*2] : 0) ){ return -1; }` |
|    28 |  287 | `				groups += 2;` |
|    15 |  288 | `			}else{` |
|   476 |  289 | `				int val = 0;` |
|   476 |  290 | `				if( segLen>4 ){ return -1; }` |
|   476 |  291 | `				if( groups>7 ){ return -1; }` |
|  1598 |  292 | `				for( j=segStart; j<i; j++ ){` |
|  1128 |  293 | `					int h = SyHexToint((unsigned char)z[j]);` |
|  1128 |  294 | `					if( h<0 ){ return -1; }` |
|  1125 |  295 | `					val = val*16 + h;` |
|   564 |  296 | `				}` |
|   473 |  297 | `				if( aOut ){` |
|   473 |  298 | `					aOut[groups*2]   = (unsigned char)(val>>8);` |
|   473 |  299 | `					aOut[groups*2+1] = (unsigned char)(val & 0xFF);` |
|   235 |  300 | `				}` |
|   473 |  301 | `				groups++;` |
|     - |  302 | `			}` |
|   499 |  303 | `			segStart = i+1;` |
|   248 |  304 | `		}` |
|  1878 |  305 | `		i++;` |
|     4 |  306 | `	}` |
|   385 |  307 | `	return groups;` |
|   199 |  308 | `}` |
|     - |  309 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present.` |
|     - |  310 | ` * The 16 address bytes land in aOut[] when it is not NULL. */` |
|   244 |  311 | `static int FvValidateIp6(const char *z,int n,unsigned char *aOut){` |
|   244 |  312 | `	const char *zDbl = 0;` |
|     - |  313 | `	int i, ga, gb;` |
|     - |  314 | `	unsigned char aHead[16], aTail[16];` |
|  1956 |  315 | `	for( i=0; i+1<n; i++ ){` |
|  1718 |  316 | `		if( z[i]==':' && z[i+1]==':' ){` |
|   233 |  317 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|   231 |  318 | `			zDbl = z+i;` |
|   114 |  319 | `		}` |
|   860 |  320 | `	}` |
|   242 |  321 | `	SyZero(aHead,(sxu32)sizeof(aHead));` |
|   242 |  322 | `	SyZero(aTail,(sxu32)sizeof(aTail));` |
|   242 |  323 | `	if( zDbl==0 ){` |
|    15 |  324 | `		if( FvIp6Hextets(z,n,aHead)!=8 ){ return 0; }` |
|   ! 0 |  325 | `		if( aOut ){ SyMemcpy(aHead,aOut,16); }` |
|   ! 0 |  326 | `		return 1;` |
|   ! 0 |  327 | `	}else{` |
|   229 |  328 | `		int lenA = (int)(zDbl - z);` |
|   229 |  329 | `		int lenB = n - lenA - 2;` |
|   229 |  330 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA,aHead);` |
|   229 |  331 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB,aTail);` |
|   229 |  332 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|   229 |  333 | `		if( (ga+gb)>7 ){ return 0; } /* "::" stands for at least one zero group */` |
|   229 |  334 | `		if( aOut ){` |
|     - |  335 | `			/* the head groups, then the elided zeros, then the tail groups */` |
|   220 |  336 | `			SyZero(aOut,16);` |
|   220 |  337 | `			if( ga>0 ){ SyMemcpy(aHead,aOut,(sxu32)(ga*2)); }` |
|   220 |  338 | `			if( gb>0 ){ SyMemcpy(aTail,&aOut[16-gb*2],(sxu32)(gb*2)); }` |
|   109 |  339 | `		}` |
|   229 |  340 | `		return 1;` |
|     - |  341 | `	}` |
|   124 |  342 | `}` |
|     - |  343 | `/*` |
|     - |  344 | ` * php's three IP RANGE flags, which describe what an address is FOR rather than` |
|     - |  345 | ` * how it is spelled. Every boundary below was derived by sweeping php 8.5 across` |
|     - |  346 | ` * the whole IPv4 space and across the IPv6 first/second hextet space.` |
|     - |  347 | ` *` |
|     - |  348 | ` *   NO_PRIV_RANGE   RFC1918: 10/8, 172.16/12, 192.168/16 -- and fc00::/7.` |
|     - |  349 | ` *   NO_RES_RANGE    0/8, 127/8, 169.254/16, 240/4 -- and ::, ::1,` |
|     - |  350 | ` *                   ::ffff:0:0/96, ::ffff:0:0:0/96, fe80::/10.` |
|     - |  351 | ` *   GLOBAL_RANGE    "not globally reachable": both sets above, plus the` |
|     - |  352 | ` *                   shared / benchmarking / documentation blocks.` |
|     - |  353 | ` */` |
|   328 |  354 | `static int FvIpRangeOk4(const unsigned char *ip,int flags){` |
|     - |  355 | `	int priv, res;` |
|   328 |  356 | `	if( (flags & (FV_FLAG_NO_PRIV_RANGE\|FV_FLAG_NO_RES_RANGE\|FV_FLAG_GLOBAL_RANGE))==0 ){` |
|    70 |  357 | `		return 1;` |
|     - |  358 | `	}` |
|   513 |  359 | `	priv = (ip[0]==10)` |
|   254 |  360 | `	    \|\| (ip[0]==172 && ip[1]>=16 && ip[1]<=31)` |
|   383 |  361 | `	    \|\| (ip[0]==192 && ip[1]==168);` |
|   376 |  362 | `	res  = (ip[0]==0) \|\| (ip[0]==127)` |
|   246 |  363 | `	    \|\| (ip[0]==169 && ip[1]==254)` |
|   383 |  364 | `	    \|\| (ip[0]>=240);` |
|   259 |  365 | `	if( (flags & FV_FLAG_NO_PRIV_RANGE) && priv ){ return 0; }` |
|   243 |  366 | `	if( (flags & FV_FLAG_NO_RES_RANGE) && res ){ return 0; }` |
|   223 |  367 | `	if( flags & FV_FLAG_GLOBAL_RANGE ){` |
|    67 |  368 | `		if( priv \|\| res ){ return 0; }` |
|    49 |  369 | `		if( ip[0]==100 && ip[1]>=64 && ip[1]<=127 ){ return 0; }            /* 100.64/10 shared */` |
|    45 |  370 | `		if( ip[0]==192 && ip[1]==0 && (ip[2]==0 \|\| ip[2]==2) ){ return 0; } /* 192.0.0/24, TEST-NET-1 */` |
|    41 |  371 | `		if( ip[0]==198 && (ip[1]==18 \|\| ip[1]==19) ){ return 0; }           /* 198.18/15 benchmarking */` |
|    37 |  372 | `		if( ip[0]==198 && ip[1]==51 && ip[2]==100 ){ return 0; }            /* TEST-NET-2 */` |
|    35 |  373 | `		if( ip[0]==203 && ip[1]==0 && ip[2]==113 ){ return 0; }             /* TEST-NET-3 */` |
|    16 |  374 | `	}` |
|   189 |  375 | `	return 1;` |
|   165 |  376 | `}` |
|   469 |  377 | `static int FvIp6BytesZero(const unsigned char *ip,int iFrom,int iTo){` |
|     - |  378 | `	int i;` |
|  1347 |  379 | `	for( i=iFrom; i<iTo; i++ ){ if( ip[i] ){ return 0; } }` |
|    59 |  380 | `	return 1;` |
|   235 |  381 | `}` |
|   220 |  382 | `static int FvIpRangeOk6(const unsigned char *ip,int flags){` |
|     - |  383 | `	int priv, res;` |
|   220 |  384 | `	if( (flags & (FV_FLAG_NO_PRIV_RANGE\|FV_FLAG_NO_RES_RANGE\|FV_FLAG_GLOBAL_RANGE))==0 ){` |
|    52 |  385 | `		return 1;` |
|     - |  386 | `	}` |
|   169 |  387 | `	priv = (ip[0] & 0xFE) == 0xFC;                                          /* fc00::/7 */` |
|   253 |  388 | `	res  = (FvIp6BytesZero(ip,0,15) && (ip[15]==0 \|\| ip[15]==1))            /* ::, ::1 */` |
|   156 |  389 | `	    \|\| (FvIp6BytesZero(ip,0,10) && ip[10]==0xFF && ip[11]==0xFF)        /* ::ffff:0:0/96 */` |
|   144 |  390 | `	    \|\| (FvIp6BytesZero(ip,0,8) && ip[8]==0xFF && ip[9]==0xFF` |
|     8 |  391 | `	        && ip[10]==0 && ip[11]==0)                                      /* ::ffff:0:0:0/96 */` |
|   252 |  392 | `	    \|\| (ip[0]==0xFE && (ip[1] & 0xC0)==0x80);                           /* fe80::/10 */` |
|   169 |  393 | `	if( (flags & FV_FLAG_NO_PRIV_RANGE) && priv ){ return 0; }` |
|   161 |  394 | `	if( (flags & FV_FLAG_NO_RES_RANGE) && res ){ return 0; }` |
|   137 |  395 | `	if( flags & FV_FLAG_GLOBAL_RANGE ){` |
|    43 |  396 | `		if( priv \|\| res ){ return 0; }` |
|    27 |  397 | `		if( ip[0]==0x20 && ip[1]==0x01 && (ip[2] & 0xFE)==0 ){ return 0; }  /* 2001::/23 protocol assignments */` |
|    23 |  398 | `		if( ip[0]==0x20 && ip[1]==0x01 && ip[2]==0x0D && ip[3]==0xB8 ){ return 0; } /* 2001:db8::/32 documentation */` |
|    21 |  399 | `		if( ip[0]==0x20 && ip[1]==0x02 ){ return 0; }                       /* 2002::/16 6to4 */` |
|    19 |  400 | `		if( ip[0]==0x01 && ip[1]==0x00 && FvIp6BytesZero(ip,2,8) ){ return 0; } /* 100::/64 discard */` |
|     8 |  401 | `	}` |
|   111 |  402 | `	return 1;` |
|   111 |  403 | `}` |
|   565 |  404 | `static int FvValidateIp(const char *z,int n,int flags){` |
|   565 |  405 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     - |  406 | `	unsigned char aIp[16];` |
|   565 |  407 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|   565 |  408 | `	if( v4 && FvValidateIp4(z,n,aIp) ){ return FvIpRangeOk4(aIp,flags); }` |
|   239 |  409 | `	if( v6 && FvValidateIp6(z,n,aIp) ){ return FvIpRangeOk6(aIp,flags); }` |
|    21 |  410 | `	return 0;` |
|   284 |  411 | `}` |
|     - |  412 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|    37 |  413 | `static int FvValidateMac(const char *z,int n){` |
|     - |  414 | `	int i;` |
|    37 |  415 | `	if( n==17 ){` |
|     - |  416 | `		/* the two byte-per-group spellings: XX:XX:XX:XX:XX:XX and XX-XX-...-XX,` |
|     - |  417 | `		 * with the SAME separator throughout */` |
|    14 |  418 | `		char sep = z[2];` |
|    14 |  419 | `		if( sep!=':' && sep!='-' ){ return 0; }` |
|   190 |  420 | `		for( i=0; i<17; i++ ){` |
|   182 |  421 | `			if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|   128 |  422 | `			else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|    90 |  423 | `		}` |
|    10 |  424 | `		return 1;` |
|     - |  425 | `	}` |
|    25 |  426 | `	if( n==14 ){` |
|     - |  427 | `		/* php's third spelling, the dotted one Cisco writes: XXXX.XXXX.XXXX */` |
|   104 |  428 | `		for( i=0; i<14; i++ ){` |
|   100 |  429 | `			if( i==4 \|\| i==9 ){ if( z[i]!='.' ){ return 0; } }` |
|    86 |  430 | `			else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|    48 |  431 | `		}` |
|     5 |  432 | `		return 1;` |
|     - |  433 | `	}` |
|    15 |  434 | `	return 0;` |
|    20 |  435 | `}` |
|     - |  436 | `#ifdef PH7_ENABLE_PCRE` |
|     - |  437 | `/*` |
|     - |  438 | ` * FILTER_VALIDATE_EMAIL is a REGEX in php — one built on Michael Rushton's, cut` |
|     - |  439 | ` * down to routable addresses — and the two spellings below are php 8.5's own,` |
|     - |  440 | ` * byte for byte: the second is the one FILTER_FLAG_EMAIL_UNICODE selects, and` |
|     - |  441 | ` * differs only by \pL\pN in the local-part classes (with /u, so a non-ASCII` |
|     - |  442 | ` * local part is a letter rather than a stray byte).` |
|     - |  443 | ` *` |
|     - |  444 | ` * What the hand-rolled screen this replaces could not say: a quoted local part` |
|     - |  445 | `` * (`"a@b"@c.com`), an address literal domain (`a@[127.0.0.1]`, `a@[IPv6:::1]`),`` |
|     - |  446 | ` * the 64-byte local-part and 254-byte total limits, the rule that a bare IPv4` |
|     - |  447 | `` * domain is not an address (`a@1.2.3.4`), and the one that the last label must`` |
|     - |  448 | ` * start with a letter. It accepted a non-ASCII local part unconditionally,` |
|     - |  449 | ` * which is the EMAIL_UNICODE answer for a program that did not ask for it.` |
|     - |  450 | ` */` |
|     - |  451 | `static const char zFvEmailRe[] =` |
|     - |  452 | `	"/^(?!(?:(?:\\x22?\\x5C[\\x00-\\x7E]\\x22?)\|(?:\\x22?[^\\x5C\\x22]\\x22?)){255,})(?!(?:(?:\\x22?\\x5C"` |
|     - |  453 | `	"[\\x00-\\x7E]\\x22?)\|(?:\\x22?[^\\x5C\\x22]\\x22?)){65,}@)(?:(?:[\\x21\\x23-\\x27\\x2A\\x2B\\x2D\\x2F"` |
|     - |  454 | `	"-\\x39\\x3D\\x3F\\x5E-\\x7E]+)\|(?:\\x22(?:[\\x01-\\x08\\x0B\\x0C\\x0E-\\x1F\\x21\\x23-\\x5B\\x5D-\\x"` |
|     - |  455 | `	"7F]\|(?:\\x5C[\\x00-\\x7F]))*\\x22))(?:\\.(?:(?:[\\x21\\x23-\\x27\\x2A\\x2B\\x2D\\x2F-\\x39\\x3D\\x3F"` |
|     - |  456 | `	"\\x5E-\\x7E]+)\|(?:\\x22(?:[\\x01-\\x08\\x0B\\x0C\\x0E-\\x1F\\x21\\x23-\\x5B\\x5D-\\x7F]\|(?:\\x5C[\\x"` |
|     - |  457 | `	"00-\\x7F]))*\\x22)))*@(?:(?:(?!.*[^.]{64,})(?:(?:(?:xn--)?[a-z0-9]+(?:-+[a-z0-9]+)*\\.){1,126}){1,}("` |
|     - |  458 | `	"?:(?:[a-z][a-z0-9]*)\|(?:(?:xn--)[a-z0-9]+))(?:-+[a-z0-9]+)*)\|(?:\\[(?:(?:IPv6:(?:(?:[a-f0-9]{1,4}(?:"` |
|     - |  459 | `	":[a-f0-9]{1,4}){7})\|(?:(?!(?:.*[a-f0-9][:\\]]){7,})(?:[a-f0-9]{1,4}(?::[a-f0-9]{1,4}){0,5})?::(?:[a-"` |
|     - |  460 | `	"f0-9]{1,4}(?::[a-f0-9]{1,4}){0,5})?)))\|(?:(?:IPv6:(?:(?:[a-f0-9]{1,4}(?::[a-f0-9]{1,4}){5}:)\|(?:(?!("` |
|     - |  461 | `	"?:.*[a-f0-9]:){5,})(?:[a-f0-9]{1,4}(?::[a-f0-9]{1,4}){0,3})?::(?:[a-f0-9]{1,4}(?::[a-f0-9]{1,4}){0,3"` |
|     - |  462 | `	"}:)?)))?(?:(?:25[0-5])\|(?:2[0-4][0-9])\|(?:1[0-9]{2})\|(?:[1-9]?[0-9]))(?:\\.(?:(?:25[0-5])\|(?:2[0-4]["` |
|     - |  463 | `	"0-9])\|(?:1[0-9]{2})\|(?:[1-9]?[0-9]))){3}))\\]))$/iD";` |
|     - |  464 | `static const char zFvEmailReUni[] =` |
|     - |  465 | `	"/^(?!(?:(?:\\x22?\\x5C[\\x00-\\x7E]\\x22?)\|(?:\\x22?[^\\x5C\\x22]\\x22?)){255,})(?!(?:(?:\\x22?\\x5C"` |
|     - |  466 | `	"[\\x00-\\x7E]\\x22?)\|(?:\\x22?[^\\x5C\\x22]\\x22?)){65,}@)(?:(?:[\\x21\\x23-\\x27\\x2A\\x2B\\x2D\\x2F"` |
|     - |  467 | `	"-\\x39\\x3D\\x3F\\x5E-\\x7E\\pL\\pN]+)\|(?:\\x22(?:[\\x01-\\x08\\x0B\\x0C\\x0E-\\x1F\\x21\\x23-\\x5B\\x"` |
|     - |  468 | `	"5D-\\x7F\\pL\\pN]\|(?:\\x5C[\\x00-\\x7F]))*\\x22))(?:\\.(?:(?:[\\x21\\x23-\\x27\\x2A\\x2B\\x2D\\x2F-\\x"` |
|     - |  469 | `	"39\\x3D\\x3F\\x5E-\\x7E\\pL\\pN]+)\|(?:\\x22(?:[\\x01-\\x08\\x0B\\x0C\\x0E-\\x1F\\x21\\x23-\\x5B\\x5D"` |
|     - |  470 | `	"-\\x7F\\pL\\pN]\|(?:\\x5C[\\x00-\\x7F]))*\\x22)))*@(?:(?:(?!.*[^.]{64,})(?:(?:(?:xn--)?[a-z0-9]+(?:-+"` |
|     - |  471 | `	"[a-z0-9]+)*\\.){1,126}){1,}(?:(?:[a-z][a-z0-9]*)\|(?:(?:xn--)[a-z0-9]+))(?:-+[a-z0-9]+)*)\|(?:\\[(?:(?"` |
|     - |  472 | `	":IPv6:(?:(?:[a-f0-9]{1,4}(?::[a-f0-9]{1,4}){7})\|(?:(?!(?:.*[a-f0-9][:\\]]){7,})(?:[a-f0-9]{1,4}(?::["` |
|     - |  473 | `	"a-f0-9]{1,4}){0,5})?::(?:[a-f0-9]{1,4}(?::[a-f0-9]{1,4}){0,5})?)))\|(?:(?:IPv6:(?:(?:[a-f0-9]{1,4}(?:"` |
|     - |  474 | `	":[a-f0-9]{1,4}){5}:)\|(?:(?!(?:.*[a-f0-9]:){5,})(?:[a-f0-9]{1,4}(?::[a-f0-9]{1,4}){0,3})?::(?:[a-f0-9"` |
|     - |  475 | `	"]{1,4}(?::[a-f0-9]{1,4}){0,3}:)?)))?(?:(?:25[0-5])\|(?:2[0-4][0-9])\|(?:1[0-9]{2})\|(?:[1-9]?[0-9]))(?:"` |
|     - |  476 | `	"\\.(?:(?:25[0-5])\|(?:2[0-4][0-9])\|(?:1[0-9]{2})\|(?:[1-9]?[0-9]))){3}))\\]))$/iDu";` |
|     - |  477 | `#endif /* PH7_ENABLE_PCRE */` |
|     - |  478 | `/*` |
|     - |  479 | ` * FILTER_VALIDATE_EMAIL. The regex above is the whole rule wherever PCRE is` |
|     - |  480 | ` * built in; a PCRE-less build (the tiny target) keeps the approximation this` |
|     - |  481 | ` * engine has always had, which is the same trade the REGEXP filter makes.` |
|     - |  482 | ` */` |
|   204 |  483 | `static int FvValidateEmail(ph7_context *pCtx,const char *z,int n,int flags){` |
|     - |  484 | `	/* The maximum length of an e-mail address is 320 octets, per RFC 2821. */` |
|   204 |  485 | `	if( n>320 ){ return 0; }` |
|     - |  486 | `#ifdef PH7_ENABLE_PCRE` |
|     - |  487 | `	{` |
|   204 |  488 | `		const char *zRe = (flags & FV_FLAG_EMAIL_UNICODE) ? zFvEmailReUni : zFvEmailRe;` |
|   204 |  489 | `		int matched = 0;` |
|   204 |  490 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,(int)SyStrlen(zRe),z,n,&matched)!=SXRET_OK ){` |
|   ! 0 |  491 | `			return 0;` |
|     - |  492 | `		}` |
|   204 |  493 | `		return matched;` |
|     - |  494 | `	}` |
|     - |  495 | `#else` |
|     - |  496 | `	{` |
|     - |  497 | `		int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|     - |  498 | `		const char *zDom;` |
|     - |  499 | `		SXUNUSED(pCtx); SXUNUSED(flags);` |
|     - |  500 | `		if( n==0 ){ return 0; }` |
|     - |  501 | `		for( i=0; i<n; i++ ){` |
|     - |  502 | `			if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     - |  503 | `		}` |
|     - |  504 | `		if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     - |  505 | `		localLen = at;` |
|     - |  506 | `		zDom = z + at + 1;` |
|     - |  507 | `		domLen = n - at - 1;` |
|     - |  508 | `		if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     - |  509 | `		for( i=0; i<localLen; i++ ){` |
|     - |  510 | `			unsigned char c = (unsigned char)z[i];` |
|     - |  511 | `			if( c<=' ' ){ return 0; }` |
|     - |  512 | `			if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     - |  513 | `		}` |
|     - |  514 | `		if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     - |  515 | `		labelStart = 0;` |
|     - |  516 | `		for( i=0; i<=domLen; i++ ){` |
|     - |  517 | `			if( i==domLen \|\| zDom[i]=='.' ){` |
|     - |  518 | `				int ll = i - labelStart;` |
|     - |  519 | `				if( ll==0 ){ return 0; } /* consecutive dots */` |
|     - |  520 | `				if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     - |  521 | `				if( i<domLen ){ dotCount++; }` |
|     - |  522 | `				labelStart = i+1;` |
|     - |  523 | `			}else{` |
|     - |  524 | `				unsigned char c = (unsigned char)zDom[i];` |
|     - |  525 | `				if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|     - |  526 | `			}` |
|     - |  527 | `		}` |
|     - |  528 | `		if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|     - |  529 | `		return 1;` |
|     - |  530 | `	}` |
|     - |  531 | `#endif /* PH7_ENABLE_PCRE */` |
|   104 |  532 | `}` |
|  1632 |  533 | `static int FvIsAlnum(unsigned char c){` |
|  1632 |  534 | `	return (c>='a'&&c<='z') \|\| (c>='A'&&c<='Z') \|\| (c>='0'&&c<='9');` |
|     4 |  535 | `}` |
|     - |  536 | `/*` |
|     - |  537 | ` * FILTER_VALIDATE_DOMAIN, a byte-for-byte port of php's own walk.` |
|     - |  538 | ` *` |
|     - |  539 | ` * Without FILTER_FLAG_HOSTNAME php checks LENGTHS and nothing else — one` |
|     - |  540 | ` * trailing dot is ignored, the rest is at most 253 bytes and splits into labels` |
|     - |  541 | ``  * of at most 63 with no empty one — so `" spaced "`, `"a b"`, `"ex@mple.com"` `` |
|     - |  542 | ` * and the empty string are all domains there. The flag adds the host-NAME rule:` |
|     - |  543 | ` * a label carries only alphanumerics and hyphens and does not start or end with` |
|     - |  544 | ` * one of the hyphens.` |
|     - |  545 | ` *` |
|     - |  546 | ` * Two edges follow from php walking the ORIGINAL string rather than a trimmed` |
|     - |  547 | ` * copy: a lone "." fails on the first-character test (the trailing-dot rule` |
|     - |  548 | ` * moved the END, not the start), and a hyphen is refused only when the byte` |
|     - |  549 | `` * after it is the string's NUL — so `"0-"` is not a hostname and `"0-."` is.`` |
|     - |  550 | ` */` |
|   265 |  551 | `static int FvValidateDomain(const char *z,int n,int flags){` |
|   265 |  552 | `	int hostname = (flags & FV_FLAG_HOSTNAME) != 0;` |
|   265 |  553 | `	int i, iEnd = n, nLen = n, nLabel = 1;` |
|   265 |  554 | `	if( n>0 && z[n-1]=='.' ){ iEnd = n-1; nLen = n-1; } /* the final dot is not part of the name */` |
|   265 |  555 | `	if( nLen>253 ){ return 0; }` |
|     - |  556 | `	/* php reads the first byte even for the empty string, where it is the NUL` |
|     - |  557 | `	 * terminator: "" is a domain, and is not a hostname. */` |
|   263 |  558 | `	if( n>0 && z[0]=='.' ){ return 0; }` |
|   249 |  559 | `	if( hostname && !(n>0 && FvIsAlnum((unsigned char)z[0])) ){ return 0; }` |
|  3349 |  560 | `	for( i=0; i<iEnd; i++ ){` |
|  3161 |  561 | `		unsigned char c = (unsigned char)z[i];` |
|  3161 |  562 | `		unsigned char nx = (i+1<n) ? (unsigned char)z[i+1] : 0; /* php's NUL byte */` |
|  3161 |  563 | `		if( c=='.' ){` |
|     - |  564 | `			/* i>0 here: a leading dot was refused above */` |
|   691 |  565 | `			if( nx=='.' \|\| (hostname && (!FvIsAlnum((unsigned char)z[i-1]) \|\| !FvIsAlnum(nx))) ){` |
|    14 |  566 | `				return 0;` |
|     - |  567 | `			}` |
|   679 |  568 | `			nLabel = 1;` |
|   341 |  569 | `		}else{` |
|  2473 |  570 | `			if( nLabel>63 \|\| (hostname && (c!='-' \|\| nx==0) && !FvIsAlnum(c)) ){ return 0; }` |
|  2447 |  571 | `			nLabel++;` |
|     - |  572 | `		}` |
|  1563 |  573 | `	}` |
|   191 |  574 | `	return 1;` |
|   134 |  575 | `}` |
|     - |  576 | `/* The bytes php's SANITIZE_URL map keeps: every printable ASCII byte. Shared` |
|     - |  577 | ` * with the URL VALIDATION below, which php runs that map over first. */` |
|  4351 |  578 | `static int FvUrlAllowed(unsigned char c){` |
|  4351 |  579 | `	return c>=33 && c<=126;` |
|     3 |  580 | `}` |
|     - |  581 | `/*` |
|     - |  582 | ` * php's userinfo rule: the bytes it lets stand in a URL's user and password.` |
|     - |  583 | `` * A percent escape is `%` + a DIGIT + a hex digit, which is php's own asymmetry`` |
|     - |  584 | ` * and not a transcription slip.` |
|     - |  585 | ` */` |
|    30 |  586 | `static int FvUserinfoValid(const char *z,int n){` |
|    30 |  587 | `	int i = 0;` |
|    62 |  588 | `	while( i<n ){` |
|    34 |  589 | `		unsigned char c = (unsigned char)z[i];` |
|    32 |  590 | `		if( FvIsAlnum(c) \|\| c=='-' \|\| c=='.' \|\| c=='_' \|\| c=='~' \|\| c=='!' \|\| c=='$'` |
|   ! 0 |  591 | `		 \|\| c=='&' \|\| c=='\'' \|\| c=='(' \|\| c==')' \|\| c=='*' \|\| c=='+' \|\| c==','` |
|     2 |  592 | `		 \|\| c==';' \|\| c=='=' \|\| c==':' ){` |
|    34 |  593 | `			i++;` |
|    18 |  594 | `		}else if( c=='%' && i+2<n && SyisDigit((unsigned char)z[i+1])` |
|   ! 0 |  595 | `		       && SyHexToint((unsigned char)z[i+2])>=0 ){` |
|   ! 0 |  596 | `			i += 3;` |
|   ! 0 |  597 | `		}else{` |
|   ! 0 |  598 | `			return 0;` |
|     - |  599 | `		}` |
|     2 |  600 | `	}` |
|    30 |  601 | `	return 1;` |
|    16 |  602 | `}` |
|   334 |  603 | `static int FvSyStrEqNoCase(const SyString *pStr,const char *zLit){` |
|   334 |  604 | `	sxu32 n = SyStrlen(zLit);` |
|   334 |  605 | `	return pStr->nByte==n && SyStrnicmp(pStr->zString,zLit,n)==0;` |
|     2 |  606 | `}` |
|   145 |  607 | `static int FvSyStrEq(const SyString *pStr,const char *zLit){` |
|   145 |  608 | `	sxu32 n = SyStrlen(zLit);` |
|   145 |  609 | `	return pStr->nByte==n && SyMemcmp(pStr->zString,zLit,n)==0;` |
|     1 |  610 | `}` |
|     - |  611 | `/*` |
|     - |  612 | ` * FILTER_VALIDATE_URL, php's own sequence.` |
|     - |  613 | ` *` |
|     - |  614 | ` * php runs the SANITIZE_URL map over the value FIRST and fails the validation` |
|     - |  615 | ` * when a byte was dropped -- which is what refuses a space, a newline or a` |
|     - |  616 | ` * non-ASCII byte anywhere in the name. Then parse_url must succeed and answer a` |
|     - |  617 | ` * SCHEME; a host is required for every scheme except the three php names` |
|     - |  618 | `` * (`mailto:`, `news:`, `file:`), and for http/https the host must be either a`` |
|     - |  619 | ` * bracketed IPv6 literal or a name that passes the HOSTNAME domain rule -- so` |
|     - |  620 | `` * `http://x_y.com/` is not a URL while `x-y://x_y.com/` is. PATH_REQUIRED and`` |
|     - |  621 | ` * QUERY_REQUIRED test the presence of those two components, and a user or` |
|     - |  622 | ` * password that is present has to be spellable.` |
|     - |  623 | ` */` |
|   323 |  624 | `static int FvValidateUrl(const char *z,int n,int flags){` |
|     - |  625 | `	VmUrlParts sUrl;` |
|     - |  626 | `	int i;` |
|   323 |  627 | `	if( n==0 ){ return 0; }` |
|  4617 |  628 | `	for( i=0; i<n; i++ ){` |
|  4329 |  629 | `		if( !FvUrlAllowed((unsigned char)z[i]) ){ return 0; }` |
|  2150 |  630 | `	}` |
|   291 |  631 | `	SyZero(&sUrl,(sxu32)sizeof(sUrl));` |
|   291 |  632 | `	if( !PH7_VmUrlSplit(z,n,&sUrl) ){ return 0; }` |
|   257 |  633 | `	if( !sUrl.bScheme ){ return 0; }` |
|   226 |  634 | `	if( FvSyStrEqNoCase(&sUrl.sScheme,"http") \|\| FvSyStrEqNoCase(&sUrl.sScheme,"https") ){` |
|     - |  635 | `		int bIp6;` |
|   128 |  636 | `		if( !sUrl.bHost ){ return 0; }` |
|   128 |  637 | `		bIp6 = sUrl.sHost.nByte>2 && sUrl.sHost.zString[0]=='['` |
|    63 |  638 | `		    && sUrl.sHost.zString[sUrl.sHost.nByte-1]==']'` |
|   185 |  639 | `		    && FvValidateIp6(sUrl.sHost.zString+1,(int)sUrl.sHost.nByte-2,0);` |
|   128 |  640 | `		if( !bIp6 && !FvValidateDomain(sUrl.sHost.zString,(int)sUrl.sHost.nByte,FV_FLAG_HOSTNAME) ){` |
|    17 |  641 | `			return 0;` |
|     - |  642 | `		}` |
|    55 |  643 | `	}` |
|   208 |  644 | `	if( !sUrl.bHost && !FvSyStrEq(&sUrl.sScheme,"mailto")` |
|    54 |  645 | `	 && !FvSyStrEq(&sUrl.sScheme,"news") && !FvSyStrEq(&sUrl.sScheme,"file") ){` |
|    33 |  646 | `		return 0;` |
|     - |  647 | `	}` |
|   178 |  648 | `	if( (flags & FV_FLAG_PATH_REQUIRED) && !sUrl.bPath ){ return 0; }` |
|   154 |  649 | `	if( (flags & FV_FLAG_QUERY_REQUIRED) && !sUrl.bQuery ){ return 0; }` |
|    96 |  650 | `	if( sUrl.bUser && !FvUserinfoValid(sUrl.sUser.zString,(int)sUrl.sUser.nByte) ){ return 0; }` |
|    96 |  651 | `	if( sUrl.bPass && !FvUserinfoValid(sUrl.sPass.zString,(int)sUrl.sPass.nByte) ){ return 0; }` |
|    96 |  652 | `	return 1;` |
|   163 |  653 | `}` |
|     - |  654 | `/* The Fv sanitizers build their result by appending directly to the call` |
|     - |  655 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|     - |  656 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|     - |  657 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|    42 |  658 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|    42 |  659 | `	int i, runStart = 0;` |
|    42 |  660 | `	ph7_result_string(pCtx,"",0);` |
|   104 |  661 | `	for( i=0; i<n; i++ ){` |
|    96 |  662 | `		char c = z[i];` |
|    96 |  663 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|    96 |  664 | `		if( !keep && isFloat ){` |
|    38 |  665 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|    23 |  666 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|    36 |  667 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|    12 |  668 | `		}` |
|    64 |  669 | `		if( !keep ){` |
|    36 |  670 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|    36 |  671 | `			runStart = i+1;` |
|    17 |  672 | `		}` |
|    33 |  673 | `	}` |
|    10 |  674 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|    10 |  675 | `}` |
|     - |  676 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|     - |  677 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|     - |  678 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|     - |  679 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|   637 |  680 | `static int FvStripByte(unsigned char c,int flags){` |
|   637 |  681 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|   627 |  682 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|   611 |  683 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|   605 |  684 | `	return 0;` |
|   320 |  685 | `}` |
|     - |  686 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|     - |  687 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|     - |  688 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|     - |  689 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|     - |  690 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|     - |  691 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|     - |  692 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|    25 |  693 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|    25 |  694 | `	int i, runStart = 0;` |
|    25 |  695 | `	ph7_result_string(pCtx,"",0);` |
|   193 |  696 | `	for( i=0; i<n; i++ ){` |
|   179 |  697 | `		unsigned char c = (unsigned char)z[i];` |
|   179 |  698 | `		if( FvStripByte(c,flags) ){` |
|    13 |  699 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|    13 |  700 | `			runStart = i+1;` |
|    13 |  701 | `			continue;` |
|     - |  702 | `		}` |
|   167 |  703 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|     3 |  704 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     3 |  705 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|     3 |  706 | `			runStart = i+1;` |
|   166 |  707 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|   164 |  708 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|    37 |  709 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     9 |  710 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     9 |  711 | `			runStart = i+1;` |
|     4 |  712 | `		}` |
|    79 |  713 | `	}` |
|    15 |  714 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|    15 |  715 | `}` |
|     - |  716 | `/*` |
|     - |  717 | ` * The strip-then-encode pass php runs before SANITIZE_STRING's strip_tags, into` |
|     - |  718 | ` * a blob rather than the call context because there is a second transform after` |
|     - |  719 | ` * it. bQuotes carries the NO_ENCODE_QUOTES decision; the rest is the same` |
|     - |  720 | ` * strip/encode precedence FvSanitizeString() applies.` |
|     - |  721 | ` */` |
|    26 |  722 | `static void FvStripEncodeBlob(SyBlob *pOut,const char *z,int n,int flags,int bQuotes){` |
|     - |  723 | `	int i;` |
|   322 |  724 | `	for( i=0; i<n; i++ ){` |
|   298 |  725 | `		unsigned char c = (unsigned char)z[i];` |
|   298 |  726 | `		if( FvStripByte(c,flags) ){ continue; }` |
|   290 |  727 | `		if( (bQuotes && (c=='\'' \|\| c=='"'))` |
|   268 |  728 | `		 \|\| (c=='&' && (flags & FV_FLAG_ENCODE_AMP))` |
|   264 |  729 | `		 \|\| (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|   277 |  730 | `		 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     - |  731 | `			char zBuf[16];` |
|    34 |  732 | `			int nBuf = SyBufferFormat(zBuf,sizeof(zBuf),"&#%u;",(unsigned int)c);` |
|    34 |  733 | `			SyBlobAppend(pOut,zBuf,(sxu32)nBuf);` |
|    18 |  734 | `		}else{` |
|   260 |  735 | `			SyBlobAppend(pOut,&z[i],1);` |
|     - |  736 | `		}` |
|   147 |  737 | `	}` |
|    26 |  738 | `}` |
|     - |  739 | `/*` |
|     - |  740 | ` * FILTER_SANITIZE_STRING (php's filter id 513, whose two CONSTANT names php` |
|     - |  741 | ` * deprecated in 8.1 and §10 therefore does not define): strip, encode the` |
|     - |  742 | ` * quotes unless NO_ENCODE_QUOTES, then strip_tags -- and answer NULL rather` |
|     - |  743 | ` * than "" for an empty result under FILTER_FLAG_EMPTY_STRING_NULL.` |
|     - |  744 | ` */` |
|    26 |  745 | `static void FvSanitizeStripString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     - |  746 | `	SyBlob sEnc;` |
|    26 |  747 | `	SyBlobInit(&sEnc,&pCtx->pVm->sAllocator);` |
|    26 |  748 | `	FvStripEncodeBlob(&sEnc,z,n,flags,(flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : 1);` |
|     - |  749 | ``	/* php's filter passes allow_tag_spaces: inside SANITIZE_STRING a `<` followed`` |
|     - |  750 | `	 * by whitespace DOES open a tag, where strip_tags() itself leaves it as text. */` |
|    26 |  751 | `	PH7_StripTagsFromString(pCtx,(const char *)SyBlobData(&sEnc),(int)SyBlobLength(&sEnc),0,0,1);` |
|    26 |  752 | `	SyBlobRelease(&sEnc);` |
|    26 |  753 | `	if( ph7_context_result_buf_length(pCtx)==0 ){` |
|     9 |  754 | `		if( flags & FV_FLAG_EMPTY_STRING_NULL ){ ph7_result_null(pCtx); }` |
|     5 |  755 | `		else{ ph7_result_string(pCtx,"",0); }` |
|     4 |  756 | `	}` |
|    26 |  757 | `}` |
|     - |  758 | `/*` |
|     - |  759 | ` * FILTER_SANITIZE_ENCODED: strip, then percent-encode every byte outside` |
|     - |  760 | ` * [A-Za-z0-9-._] with php's UPPERCASE hex.` |
|     - |  761 | ` */` |
|     9 |  762 | `static void FvSanitizeEncoded(ph7_context *pCtx,const char *z,int n,int flags){` |
|     - |  763 | `	static const char zHex[] = "0123456789ABCDEF";` |
|     - |  764 | `	int i;` |
|     9 |  765 | `	ph7_result_string(pCtx,"",0);` |
|    61 |  766 | `	for( i=0; i<n; i++ ){` |
|    53 |  767 | `		unsigned char c = (unsigned char)z[i];` |
|    53 |  768 | `		if( FvStripByte(c,flags) ){ continue; }` |
|    47 |  769 | `		if( FvIsAlnum(c) \|\| c=='-' \|\| c=='.' \|\| c=='_' ){` |
|    33 |  770 | `			ph7_result_string(pCtx,&z[i],1);` |
|    17 |  771 | `		}else{` |
|     - |  772 | `			char zBuf[3];` |
|    15 |  773 | `			zBuf[0] = '%'; zBuf[1] = zHex[c>>4]; zBuf[2] = zHex[c & 15];` |
|    15 |  774 | `			ph7_result_string(pCtx,zBuf,3);` |
|     - |  775 | `		}` |
|    24 |  776 | `	}` |
|     9 |  777 | `}` |
|     - |  778 | `/*` |
|     - |  779 | ` * FILTER_SANITIZE_ADD_SLASHES: php's addslashes(), flags and all ignored.` |
|     - |  780 | ` */` |
|     7 |  781 | `static void FvSanitizeAddSlashes(ph7_context *pCtx,const char *z,int n){` |
|     7 |  782 | `	int i, runStart = 0;` |
|     7 |  783 | `	ph7_result_string(pCtx,"",0);` |
|    57 |  784 | `	for( i=0; i<n; i++ ){` |
|    51 |  785 | `		char c = z[i];` |
|    51 |  786 | `		if( c=='\'' \|\| c=='"' \|\| c=='\\' \|\| c==0 ){` |
|     9 |  787 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     9 |  788 | `			if( c==0 ){ ph7_result_string(pCtx,"\\0",2); }` |
|     7 |  789 | `			else{ ph7_result_string_format(pCtx,"\\%c",c); }` |
|     9 |  790 | `			runStart = i+1;` |
|     4 |  791 | `		}` |
|    26 |  792 | `	}` |
|     7 |  793 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     7 |  794 | `}` |
|     - |  795 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|     - |  796 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|     - |  797 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|     - |  798 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|     - |  799 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|    16 |  800 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|    16 |  801 | `	int i, runStart = 0;` |
|     - |  802 | `	const char *zEnt;` |
|    16 |  803 | `	ph7_result_string(pCtx,"",0);` |
|   134 |  804 | `	for( i=0; i<n; i++ ){` |
|   119 |  805 | `		unsigned char c = (unsigned char)z[i];` |
|   119 |  806 | `		if( FvStripByte(c,flags) ){` |
|     9 |  807 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     9 |  808 | `			runStart = i+1;` |
|     9 |  809 | `			continue;` |
|     - |  810 | `		}` |
|   111 |  811 | `		switch( c ){` |
|     3 |  812 | `		case '<':  zEnt = "&#60;"; break;` |
|     3 |  813 | `		case '>':  zEnt = "&#62;"; break;` |
|    11 |  814 | `		case '&':  zEnt = "&#38;"; break;` |
|     3 |  815 | `		case '"':  zEnt = "&#34;"; break;` |
|     3 |  816 | `		case '\'': zEnt = "&#39;"; break;` |
|    46 |  817 | `		default:` |
|     - |  818 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|     - |  819 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|    93 |  820 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|    17 |  821 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|    17 |  822 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|    17 |  823 | `				runStart = i+1;` |
|     8 |  824 | `			}` |
|    93 |  825 | `			continue; /* keep in the current run */` |
|     - |  826 | `		}` |
|    19 |  827 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|    19 |  828 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|    19 |  829 | `		runStart = i+1;` |
|    10 |  830 | `	}` |
|    16 |  831 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|    16 |  832 | `}` |
|     - |  833 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|     - |  834 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|     - |  835 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|     - |  836 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|     - |  837 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|     - |  838 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|     - |  839 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|     - |  840 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|     - |  841 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|     - |  842 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|     - |  843 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|     - |  844 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|     - |  845 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|     - |  846 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|     - |  847 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|     - |  848 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|     - |  849 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|     - |  850 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|     - |  851 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|     - |  852 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|     - |  853 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|     - |  854 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|     - |  855 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|     - |  856 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|     - |  857 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|     - |  858 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|     - |  859 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|     - |  860 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|     - |  861 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|     - |  862 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|     - |  863 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|     - |  864 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|     - |  865 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|     - |  866 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|     - |  867 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|     - |  868 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|     - |  869 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|     - |  870 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|     - |  871 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|     - |  872 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|     - |  873 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|     - |  874 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|     - |  875 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|     - |  876 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|     - |  877 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|     - |  878 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|     - |  879 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|     - |  880 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|     - |  881 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|     - |  882 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|     - |  883 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|     - |  884 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|     - |  885 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|     - |  886 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|     - |  887 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|     - |  888 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|     - |  889 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|     - |  890 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|     - |  891 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|     - |  892 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|     - |  893 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|     - |  894 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|     - |  895 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|     - |  896 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|     - |  897 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|     - |  898 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|     - |  899 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|     - |  900 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|     - |  901 | `};` |
|     - |  902 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|    64 |  903 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|    64 |  904 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|   498 |  905 | `	while( lo <= hi ){` |
|   476 |  906 | `		int mid = (lo + hi) / 2;` |
|   476 |  907 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|   476 |  908 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|   436 |  909 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|     2 |  910 | `	}` |
|    24 |  911 | `	return 0;` |
|    33 |  912 | `}` |
|     - |  913 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|     - |  914 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|     - |  915 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|     - |  916 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|   112 |  917 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|   112 |  918 | `	unsigned char c = p[0];` |
|   112 |  919 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|   112 |  920 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|   110 |  921 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|    56 |  922 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|    54 |  923 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|    54 |  924 | `		return 2;` |
|     - |  925 | `	}` |
|    56 |  926 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|     - |  927 | `		sxu32 cp;` |
|    47 |  928 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|    33 |  929 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|    33 |  930 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|    29 |  931 | `		*pCp = cp;` |
|    29 |  932 | `		return 3;` |
|     - |  933 | `	}` |
|    10 |  934 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|     - |  935 | `		sxu32 cp;` |
|     5 |  936 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|     5 |  937 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|     5 |  938 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|     5 |  939 | `		*pCp = cp;` |
|     5 |  940 | `		return 4;` |
|     - |  941 | `	}` |
|     6 |  942 | `	return 0;                                /* 0xF5-0xFF */` |
|    57 |  943 | `}` |
|     - |  944 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|     - |  945 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|     - |  946 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|     - |  947 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|     - |  948 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|     - |  949 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|     - |  950 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|     - |  951 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|     - |  952 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|     - |  953 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|    25 |  954 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|    25 |  955 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     - |  956 | `	/* filter_var's FULL_SPECIAL_CHARS has no charset argument: php runs it in the` |
|     - |  957 | `	 * default charset. */` |
|    25 |  958 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/,PH7_HTML_CS_UTF8);` |
|    25 |  959 | `}` |
|     - |  960 | `/* ---------------------------------------------------------------------------` |
|     - |  961 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|     - |  962 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|     - |  963 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|     - |  964 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|     - |  965 | ` * ------------------------------------------------------------------------ */` |
|     - |  966 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|     - |  967 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|  3574 |  968 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|  3574 |  969 | `	sxu8 *z = (sxu8 *)zBuf;` |
|  3574 |  970 | `	SX_WRITE_UTF8(z,cp);` |
|  3574 |  971 | `	return (int)(z - (sxu8 *)zBuf);` |
|     2 |  972 | `}` |
|     - |  973 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|     - |  974 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|     - |  975 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|     - |  976 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|     - |  977 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|     - |  978 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|   162 |  979 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|   162 |  980 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|   162 |  981 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|   158 |  982 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|   156 |  983 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|   156 |  984 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|   130 |  985 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|   128 |  986 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|   102 |  987 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|   ! 0 |  988 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|     - |  989 | `	}` |
|   102 |  990 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|     9 |  991 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|     9 |  992 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|     4 |  993 | `	}` |
|   102 |  994 | `	return 1;` |
|    82 |  995 | `}` |
|     - |  996 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|     - |  997 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|     - |  998 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|     - |  999 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|    66 | 1000 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|    66 | 1001 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|    66 | 1002 | `	return HtmlCpAllowed(cp,iFlags);` |
|    34 | 1003 | `}` |
|     - | 1004 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|     - | 1005 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|     - | 1006 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|     - | 1007 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|     - | 1008 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|     - | 1009 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|     - | 1010 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|     - | 1011 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|     9 | 1012 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|     9 | 1013 | `	if( cp > 0x10FFFF ){ return 0; }` |
|     7 | 1014 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|   ! 0 | 1015 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|   ! 0 | 1016 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|   ! 0 | 1017 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|   ! 0 | 1018 | `	return 1;` |
|     5 | 1019 | `}` |
|     - | 1020 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|     - | 1021 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|     - | 1022 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|     - | 1023 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|     - | 1024 | ` * start a new sequence is left for the next round. */` |
|     5 | 1025 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|    11 | 1026 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|    15 | 1027 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|    15 | 1028 | `	unsigned char c = p[0];` |
|    15 | 1029 | `	int nAvail = (int)(zEnd - p);` |
|    15 | 1030 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|    13 | 1031 | `	if( c < 0xE0 ){` |
|     3 | 1032 | `		if( nAvail < 2 ){ return 1; }` |
|     3 | 1033 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|     - | 1034 | `	}` |
|    11 | 1035 | `	if( c < 0xF0 ){` |
|    11 | 1036 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|     3 | 1037 | `			return 3; /* complete but overlong/surrogate */` |
|     - | 1038 | `		}` |
|     9 | 1039 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|   ! 0 | 1040 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|   ! 0 | 1041 | `		return 3;` |
|     - | 1042 | `	}` |
|   ! 0 | 1043 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|   ! 0 | 1044 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|     - | 1045 | `	}` |
|   ! 0 | 1046 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|   ! 0 | 1047 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|   ! 0 | 1048 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|   ! 0 | 1049 | `	return 4;` |
|     8 | 1050 | `}` |
|     - | 1051 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|     - | 1052 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|     - | 1053 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|     - | 1054 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|     - | 1055 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|     - | 1056 | `};` |
|     - | 1057 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|     - | 1058 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|     - | 1059 | ` * HTML 4.01 table (documented divergence). */` |
|   126 | 1060 | `static int HtmlDocHasNamedTable(int iDoc){` |
|   126 | 1061 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|     2 | 1062 | `}` |
|     - | 1063 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|     - | 1064 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|     - | 1065 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|     - | 1066 | ` * whichever function the requested table belongs to. */` |
|    52 | 1067 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|    52 | 1068 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|    44 | 1069 | `		return "&#039;";` |
|     - | 1070 | `	}` |
|     9 | 1071 | `	return "&apos;";` |
|    27 | 1072 | `}` |
|     - | 1073 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|     - | 1074 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|     - | 1075 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|     - | 1076 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|     - | 1077 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|     - | 1078 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|     - | 1079 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|     - | 1080 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|     - | 1081 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|     - | 1082 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|     - | 1083 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|     - | 1084 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|   208 | 1085 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|     2 | 1086 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|   210 | 1087 | `	int nAvail = (int)(zEnd - z);` |
|   210 | 1088 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     - | 1089 | `	sxu32 n;` |
|   210 | 1090 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|   206 | 1091 | `	if( z[1] == '#' ){` |
|     - | 1092 | `		/* Numeric reference */` |
|   104 | 1093 | `		sxu32 cp = 0;` |
|   104 | 1094 | `		int i = 2, bHex = 0, nDig = 0;` |
|   104 | 1095 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|   374 | 1096 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|     - | 1097 | `			int v;` |
|   264 | 1098 | `			unsigned char c = z[i];` |
|   264 | 1099 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|    22 | 1100 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|    22 | 1101 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|   ! 0 | 1102 | `			else { return 0; }` |
|     - | 1103 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|     - | 1104 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|   264 | 1105 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|   264 | 1106 | `			nDig++;` |
|   133 | 1107 | `		}` |
|   112 | 1108 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|   112 | 1109 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    98 | 1110 | `		if( !bFull ){` |
|     - | 1111 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|    99 | 1112 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|    25 | 1113 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|    11 | 1114 | `		}` |
|    90 | 1115 | `		*pCp = cp;` |
|    90 | 1116 | `		*pnConsumed = i + 1;` |
|    90 | 1117 | `		return 1;` |
|     - | 1118 | `	}` |
|     - | 1119 | `	/* Named reference — every entity name starts with a letter, so anything` |
|     - | 1120 | `	 * else can bail out before touching the tables. */` |
|   104 | 1121 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|   398 | 1122 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|   360 | 1123 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|   322 | 1124 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|    60 | 1125 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|    60 | 1126 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|    60 | 1127 | `			return 1;` |
|     - | 1128 | `		}` |
|   133 | 1129 | `	}` |
|    40 | 1130 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|     - | 1131 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|     - | 1132 | `		 * positions and guarantees the decode set can never drift from the` |
|     - | 1133 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|     - | 1134 | `		 * for ~96% of rows. */` |
|  5340 | 1135 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|     - | 1136 | `			sxu32 nEnt;` |
|  5326 | 1137 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|   188 | 1138 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|   188 | 1139 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|    22 | 1140 | `				*pCp = aHtml401Ent[n].cp;` |
|    22 | 1141 | `				*pnConsumed = (int)nEnt;` |
|    22 | 1142 | `				return 1;` |
|     - | 1143 | `			}` |
|    85 | 1144 | `		}` |
|     7 | 1145 | `	}` |
|    20 | 1146 | `	return 0;` |
|   107 | 1147 | `}` |
|     - | 1148 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|     - | 1149 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|     - | 1150 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|     - | 1151 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|     - | 1152 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|   160 | 1153 | `PH7_PRIVATE void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|     2 | 1154 | `                       int iFlags,int bAll,int bDoubleEncode,int iCs){` |
|   162 | 1155 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|   162 | 1156 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     - | 1157 | `	const unsigned char *runStart;` |
|   162 | 1158 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     - | 1159 | `	/* ENT_DISALLOWED replaces a character the doctype forbids with U+FFFD — as a` |
|     - | 1160 | `	 * CHARACTER where the charset can hold one, and as the numeric REFERENCE for` |
|     - | 1161 | `	 * it where it cannot, which is every single-byte charset. */` |
|   162 | 1162 | `	const char *zRepl = (iCs == PH7_HTML_CS_LATIN1) ? "&#xFFFD;" : "\xEF\xBF\xBD";` |
|     - | 1163 | `	sxu32 cp;` |
|   162 | 1164 | `	if( iCs == PH7_HTML_CS_UTF8 && (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|     - | 1165 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|     - | 1166 | `		 * bytes cannot be malformed, so skip them without the decoder.` |
|     - | 1167 | `		 * A single-byte charset has no malformed sequences to find. */` |
|   434 | 1168 | `		while( p < zEnd ){` |
|     - | 1169 | `			int len;` |
|   358 | 1170 | `			if( *p < 0x80 ){ p++; continue; }` |
|    44 | 1171 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|    44 | 1172 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|    32 | 1173 | `			p += len;` |
|     2 | 1174 | `		}` |
|    78 | 1175 | `		p = (const unsigned char *)zIn;` |
|    38 | 1176 | `	}` |
|   150 | 1177 | `	runStart = p;` |
|   150 | 1178 | `	ph7_result_string(pCtx,"",0);` |
|   634 | 1179 | `	while( p < zEnd ){` |
|   486 | 1180 | `		const char *zEnt = 0;` |
|     - | 1181 | `		int len;` |
|   486 | 1182 | `		if( *p < 0x80 ){` |
|   382 | 1183 | `			len = 1;` |
|   382 | 1184 | `			switch( *p ){` |
|    32 | 1185 | `			case '<': zEnt = "&lt;"; break;` |
|    25 | 1186 | `			case '>': zEnt = "&gt;"; break;` |
|    22 | 1187 | `			case '&':` |
|    46 | 1188 | `				zEnt = "&amp;";` |
|    46 | 1189 | `				if( !bDoubleEncode ){` |
|     - | 1190 | `					sxu32 eCp; int nEat;` |
|    28 | 1191 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|     - | 1192 | `						/* A valid existing entity: keep it verbatim. */` |
|    16 | 1193 | `						zEnt = 0;` |
|    16 | 1194 | `						len = nEat;` |
|     7 | 1195 | `					}` |
|    13 | 1196 | `				}` |
|    46 | 1197 | `				break;` |
|    10 | 1198 | `			case '"':` |
|    21 | 1199 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|    21 | 1200 | `				break;` |
|    12 | 1201 | `			case '\'':` |
|    25 | 1202 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|    23 | 1203 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|    11 | 1204 | `				}` |
|    25 | 1205 | `				break;` |
|   119 | 1206 | `			default:` |
|   240 | 1207 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    29 | 1208 | `					zEnt = zRepl;` |
|    14 | 1209 | `				}` |
|   238 | 1210 | `				break;` |
|     - | 1211 | `			}` |
|   296 | 1212 | `		}else if( iCs == PH7_HTML_CS_LATIN1 ){` |
|     - | 1213 | `			/* Every byte is a character and its value IS the code point. */` |
|    37 | 1214 | `			len = 1;` |
|    37 | 1215 | `			cp = *p;` |
|    37 | 1216 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|    19 | 1217 | `				zEnt = FvHtml401Lookup(cp);` |
|     9 | 1218 | `			}` |
|    37 | 1219 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    11 | 1220 | `				zEnt = zRepl;` |
|     5 | 1221 | `			}` |
|    19 | 1222 | `		}else{` |
|    70 | 1223 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|    70 | 1224 | `			if( len == 0 ){` |
|     - | 1225 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|     - | 1226 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|     - | 1227 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|    15 | 1228 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    15 | 1229 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|    15 | 1230 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|    15 | 1231 | `				runStart = p;` |
|    15 | 1232 | `				continue;` |
|     - | 1233 | `			}` |
|    56 | 1234 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|    46 | 1235 | `				zEnt = FvHtml401Lookup(cp);` |
|    22 | 1236 | `			}` |
|    56 | 1237 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|   ! 0 | 1238 | `				zEnt = zRepl;` |
|   ! 0 | 1239 | `			}` |
|     - | 1240 | `		}` |
|   472 | 1241 | `		if( zEnt ){` |
|   200 | 1242 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|   200 | 1243 | `			ph7_result_string(pCtx,zEnt,-1);` |
|   200 | 1244 | `			runStart = p + len;` |
|    99 | 1245 | `		}` |
|   472 | 1246 | `		p += len;` |
|     2 | 1247 | `	}` |
|   150 | 1248 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|    82 | 1249 | `}` |
|     - | 1250 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|     - | 1251 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|     - | 1252 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|     - | 1253 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|     - | 1254 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|    92 | 1255 | `PH7_PRIVATE void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|     2 | 1256 | `                         int iFlags,int bFull,int iCs){` |
|    94 | 1257 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|    94 | 1258 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|    94 | 1259 | `	const unsigned char *runStart = p;` |
|    94 | 1260 | `	ph7_result_string(pCtx,"",0);` |
|   618 | 1261 | `	while( p < zEnd ){` |
|     - | 1262 | `		sxu32 cp;` |
|     - | 1263 | `		int nEat;` |
|   566 | 1264 | `		if( *p != '&' ){ p++; continue; }` |
|   192 | 1265 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|   158 | 1266 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|   152 | 1267 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|     - | 1268 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|    39 | 1269 | `			p += nEat;` |
|    39 | 1270 | `			continue;` |
|     - | 1271 | `		}` |
|   122 | 1272 | `		if( iCs == PH7_HTML_CS_LATIN1 && cp > 0xFF ){` |
|     - | 1273 | `			/* The charset cannot hold it, so php leaves the entity SOURCE alone —` |
|     - | 1274 | ``			 * `&hearts;` stays `&hearts;` in a Latin-1 document. */`` |
|     5 | 1275 | `			p += nEat;` |
|     5 | 1276 | `			continue;` |
|     - | 1277 | `		}` |
|   118 | 1278 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|   118 | 1279 | `		if( iCs == PH7_HTML_CS_LATIN1 ){` |
|    17 | 1280 | `			char zByte = (char)cp;` |
|    17 | 1281 | `			ph7_result_string(pCtx,&zByte,1);` |
|     9 | 1282 | `		}else{` |
|     - | 1283 | `			char zBuf[4];` |
|   102 | 1284 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|   102 | 1285 | `			ph7_result_string(pCtx,zBuf,n);` |
|     - | 1286 | `		}` |
|   118 | 1287 | `		p += nEat;` |
|   118 | 1288 | `		runStart = p;` |
|     2 | 1289 | `	}` |
|    88 | 1290 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|    88 | 1291 | `}` |
|     - | 1292 | `/* Resolve the optional charset argument at apArg[idx] to a PH7_HTML_CS_* code.` |
|     - | 1293 | ` *` |
|     - | 1294 | ` * The argument was SCREENED and then dropped: every charset but UTF-8 warned and` |
|     - | 1295 | `` * was answered in UTF-8, so `htmlentities($s, ENT_QUOTES, 'ISO-8859-1')` — the`` |
|     - | 1296 | ` * ordinary call for a Latin-1 page — answered a table of 253 rows where php` |
|     - | 1297 | ` * answers 101, encoded a Latin-1 byte as a broken UTF-8 sequence, and decoded` |
|     - | 1298 | `` * `&eacute;` to two bytes where php writes one.`` |
|     - | 1299 | ` *` |
|     - | 1300 | ` * ISO-8859-1 is a real charset here now (one byte per character, and its VALUE is` |
|     - | 1301 | ` * the code point). php also supports several other single-byte charsets and the` |
|     - | 1302 | ` * Asian multibyte ones; those stay behind the same UTF-8/Latin-1/ASCII scope cut` |
|     - | 1303 | ` * the mb_ and iconv work draws, and keep php's own unsupported-charset warning. */` |
|   242 | 1304 | `PH7_PRIVATE int HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|     - | 1305 | `	const char *zCs;` |
|     - | 1306 | `	int nCs;` |
|   242 | 1307 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return PH7_HTML_CS_UTF8; }` |
|   114 | 1308 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|   114 | 1309 | `	if( nCs == 0 ){ return PH7_HTML_CS_UTF8; } /* "" selects the default charset */` |
|   110 | 1310 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|    38 | 1311 | `		return PH7_HTML_CS_UTF8; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|     - | 1312 | `	}` |
|     - | 1313 | `	/* php's own alias set for Latin-1, matched the way php matches it. */` |
|    72 | 1314 | `	if( (nCs == 10 && SyStrnicmp(zCs,"ISO-8859-1",10) == 0)` |
|    41 | 1315 | `	 \|\| (nCs == 9  && SyStrnicmp(zCs,"ISO8859-1",9) == 0) ){` |
|    67 | 1316 | `		return PH7_HTML_CS_LATIN1;` |
|     - | 1317 | `	}` |
|    13 | 1318 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     4 | 1319 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     9 | 1320 | `	return PH7_HTML_CS_UTF8;` |
|   123 | 1321 | `}` |
|     - | 1322 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|     - | 1323 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|     - | 1324 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|     - | 1325 | ` * ordering; 253 entries under the defaults). */` |
|  4816 | 1326 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|  4816 | 1327 | `	ph7_value_string(pValue,zEnt,-1);` |
|  4816 | 1328 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|  4816 | 1329 | `	ph7_value_reset_string_cursor(pValue);` |
|  4816 | 1330 | `}` |
|    44 | 1331 | `PH7_PRIVATE void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags,int iCs){` |
|     - | 1332 | `	ph7_value *pArray,*pValue;` |
|    44 | 1333 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     - | 1334 | `	sxu32 n;` |
|    44 | 1335 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    44 | 1336 | `	pArray = ph7_context_new_array(pCtx);` |
|    44 | 1337 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|   ! 0 | 1338 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1339 | `		return;` |
|     - | 1340 | `	}` |
|    44 | 1341 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|    38 | 1342 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|    18 | 1343 | `	}` |
|    44 | 1344 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|    44 | 1345 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     - | 1346 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|     - | 1347 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|     - | 1348 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|    30 | 1349 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|    14 | 1350 | `	}` |
|    44 | 1351 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|    44 | 1352 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|    44 | 1353 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|     - | 1354 | `		char zKey[8];` |
|  6476 | 1355 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|     - | 1356 | `			int nK;` |
|  6450 | 1357 | `			if( iCs == PH7_HTML_CS_LATIN1 ){` |
|     - | 1358 | `				/* One byte per character, and only the characters the charset HAS:` |
|     - | 1359 | `				 * php's Latin-1 table is the 96 rows below U+0100 plus the` |
|     - | 1360 | `				 * specials, 101 in all. */` |
|  2977 | 1361 | `				if( aHtml401Ent[n].cp > 0xFF ){` |
|  1825 | 1362 | `					continue;` |
|     - | 1363 | `				}` |
|  1153 | 1364 | `				zKey[0] = (char)aHtml401Ent[n].cp;` |
|  1153 | 1365 | `				nK = 1;` |
|   577 | 1366 | `			}else{` |
|  3474 | 1367 | `				nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|     - | 1368 | `			}` |
|  4626 | 1369 | `			zKey[nK] = 0;` |
|  4626 | 1370 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|  2314 | 1371 | `		}` |
|    13 | 1372 | `	}` |
|    44 | 1373 | `	ph7_result_value(pCtx,pArray);` |
|    23 | 1374 | `}` |
|    25 | 1375 | `static int FvEmailAllowed(unsigned char c){` |
|    25 | 1376 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|    16 | 1377 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|    10 | 1378 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|    15 | 1379 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|    13 | 1380 | `}` |
|     - | 1381 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|     5 | 1382 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|     5 | 1383 | `	int i, runStart = 0;` |
|     5 | 1384 | `	ph7_result_string(pCtx,"",0);` |
|    51 | 1385 | `	for( i=0; i<n; i++ ){` |
|    47 | 1386 | `		unsigned char c = (unsigned char)z[i];` |
|    47 | 1387 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|    11 | 1388 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|    11 | 1389 | `			runStart = i+1;` |
|     5 | 1390 | `		}` |
|    24 | 1391 | `	}` |
|     5 | 1392 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     5 | 1393 | `}` |
|     - | 1394 | `/*` |
|     - | 1395 | ` * php's filter NAME table, in php's own order: what filter_list() answers and` |
|     - | 1396 | ` * what filter_id() looks names up in. Two names share id 513 ("string" and` |
|     - | 1397 | ` * "stripped"), which is why filter_id() is a name->id map and not a bijection.` |
|     - | 1398 | ` */` |
|     - | 1399 | `static const struct FvFilterName {` |
|     - | 1400 | `	const char *zName;` |
|     - | 1401 | `	int iId;` |
|     - | 1402 | `} aFvFilterName[] = {` |
|     - | 1403 | `	{ "int",                257 }, { "boolean",            258 },` |
|     - | 1404 | `	{ "float",              259 }, { "validate_regexp",    272 },` |
|     - | 1405 | `	{ "validate_domain",    277 }, { "validate_url",       273 },` |
|     - | 1406 | `	{ "validate_email",     274 }, { "validate_ip",        275 },` |
|     - | 1407 | `	{ "validate_mac",       276 }, { "string",             513 },` |
|     - | 1408 | `	{ "stripped",           513 }, { "encoded",            514 },` |
|     - | 1409 | `	{ "special_chars",      515 }, { "full_special_chars", 522 },` |
|     - | 1410 | `	{ "unsafe_raw",         516 }, { "email",              517 },` |
|     - | 1411 | `	{ "url",                518 }, { "number_int",         519 },` |
|     - | 1412 | `	{ "number_float",       520 }, { "add_slashes",        523 },` |
|     - | 1413 | `	{ "callback",          1024 }` |
|     - | 1414 | `};` |
|     - | 1415 | `/* The NAME php words a filter's failure with. The first row wins for the two` |
|     - | 1416 | ` * ids that have two names, which is the one php's own table answers. */` |
|    20 | 1417 | `static const char * FvFilterIdName(int iFilter)` |
|     1 | 1418 | `{` |
|     - | 1419 | `	sxu32 n;` |
|    79 | 1420 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFvFilterName) ; ++n ){` |
|    79 | 1421 | `		if( aFvFilterName[n].iId == iFilter ){ return aFvFilterName[n].zName; }` |
|    30 | 1422 | `	}` |
|   ! 0 | 1423 | `	return "unsafe_raw";` |
|    11 | 1424 | `}` |
|     - | 1425 | `/* Is this a filter id php has? php answers a filter_var() with an unknown one` |
|     - | 1426 | ` * with a warning and false, and an unknown one INSIDE an array definition with` |
|     - | 1427 | ` * the warning and then the DEFAULT filter. */` |
|  1736 | 1428 | `static int FvFilterIdExists(int iFilter)` |
|     5 | 1429 | `{` |
|     - | 1430 | `	sxu32 n;` |
| 11923 | 1431 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFvFilterName) ; ++n ){` |
| 11919 | 1432 | `		if( aFvFilterName[n].iId == iFilter ){ return 1; }` |
|  5096 | 1433 | `	}` |
|     5 | 1434 | `	return 0;` |
|   873 | 1435 | `}` |
|     - | 1436 | `/*` |
|     - | 1437 | ` * The superglobal an INPUT_* constant names. Answers 0 for a constant php does` |
|     - | 1438 | ` * not have, where the caller raises php's ValueError under its own name.` |
|     - | 1439 | ` */` |
|     8 | 1440 | `static const char * FvInputSuper(int iType,sxu32 *pnLen)` |
|     1 | 1441 | `{` |
|     9 | 1442 | `	switch( iType ){` |
|   ! 0 | 1443 | `	case 0: *pnLen = (sxu32)sizeof("_POST")-1;   return "_POST";` |
|     5 | 1444 | `	case 1: *pnLen = (sxu32)sizeof("_GET")-1;    return "_GET";` |
|   ! 0 | 1445 | `	case 2: *pnLen = (sxu32)sizeof("_COOKIE")-1; return "_COOKIE";` |
|   ! 0 | 1446 | `	case 4: *pnLen = (sxu32)sizeof("_ENV")-1;    return "_ENV";` |
|   ! 0 | 1447 | `	case 5: *pnLen = (sxu32)sizeof("_SERVER")-1; return "_SERVER";` |
|     4 | 1448 | `	default: break;` |
|     - | 1449 | `	}` |
|     5 | 1450 | `	*pnLen = 0;` |
|     5 | 1451 | `	return 0;` |
|     5 | 1452 | `}` |
|     - | 1453 | `/*` |
|     - | 1454 | ` * Apply the selected filter to one already-resolved input value and write the` |
|     - | 1455 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|     - | 1456 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|     - | 1457 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|     - | 1458 | ` * else false. A validating filter that passes returns the (string) input` |
|     - | 1459 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|     - | 1460 | ` */` |
|  1758 | 1461 | `static int FvApplyFilterRaw(ph7_context *pCtx,ph7_value *pInput,` |
|     - | 1462 | `                            int iFilter,int iFlags,ph7_value *pOpts,` |
|     - | 1463 | `                            ph7_value *pDefault,const char *zFunc)` |
|     5 | 1464 | `{` |
|  1763 | 1465 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|     - | 1466 | `	const char *zVal; int nVal, rc;` |
|     - | 1467 | `	/* An array/object input fails every scalar filter. */` |
|  1763 | 1468 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|  1763 | 1469 | `	if( iFilter == FV_CALLBACK ){` |
|     - | 1470 | ``		/* php's callback filter: `options` IS the callable (not an array of`` |
|     - | 1471 | `		 * options), it is handed the value, and whatever it answers is the` |
|     - | 1472 | `		 * result. A missing or unusable one is a TypeError, and the value` |
|     - | 1473 | `		 * becomes null. */` |
|     - | 1474 | `		ph7_value *pRes;` |
|     - | 1475 | `		char zReason[256];` |
|     - | 1476 | `		/* the callable SCREEN, not the screen's own message: php words this one` |
|     - | 1477 | `		 * itself ("Option must be a valid callback") */` |
|    27 | 1478 | `		if( pOpts==0 \|\| PH7_VmCallableReason(pCtx->pVm,pOpts,zReason,sizeof(zReason))!=0 ){` |
|     7 | 1479 | `			ph7_result_null(pCtx);` |
|    10 | 1480 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     3 | 1481 | `				"%s(): Option must be a valid callback",zFunc);` |
|     - | 1482 | `		}` |
|    21 | 1483 | `		pRes = ph7_context_new_scalar(pCtx);` |
|    21 | 1484 | `		if( pRes==0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    21 | 1485 | `		if( PH7_VmCallUserFunction(pCtx->pVm,pOpts,1,&pInput,pRes)==SXRET_OK ){` |
|    19 | 1486 | `			ph7_result_value(pCtx,pRes);` |
|    10 | 1487 | `		}else{` |
|     3 | 1488 | `			ph7_result_null(pCtx);` |
|     - | 1489 | `		}` |
|    21 | 1490 | `		ph7_context_release_value(pCtx,pRes);` |
|    21 | 1491 | `		return PH7_OK;` |
|     - | 1492 | `	}` |
|  1737 | 1493 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|  1737 | 1494 | `	switch( iFilter ){` |
|    79 | 1495 | `	case FV_VALIDATE_INT: {` |
|     - | 1496 | `		ph7_int64 v;` |
|   166 | 1497 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|   103 | 1498 | `		if( pOpts ){` |
|    22 | 1499 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|    22 | 1500 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|     - | 1501 | `			/* Read through a COPY: ph7_value_to_int64() would rewrite the entry in` |
|     - | 1502 | `			 * the caller's own $options array (php's zval_get_long() does not). */` |
|    22 | 1503 | `			if( pMin && v<PH7_ValuePeekInt64(pMin) ){ goto fail; }` |
|    16 | 1504 | `			if( pMax && v>PH7_ValuePeekInt64(pMax) ){ goto fail; }` |
|     5 | 1505 | `		}` |
|    93 | 1506 | `		ph7_result_int64(pCtx,v);` |
|    93 | 1507 | `		return PH7_OK;` |
|     - | 1508 | `	}` |
|    67 | 1509 | `	case FV_VALIDATE_FLOAT: {` |
|     - | 1510 | `		double d;` |
|   136 | 1511 | `		int decSep = '.';` |
|   136 | 1512 | `		const char *zSep = "',."; int nSep = 3;` |
|     - | 1513 | `		/* php reads "decimal"/"thousand" only when the option IS a string — an int,` |
|     - | 1514 | `		 * a bool, null or an array leaves the defaults in place rather than being` |
|     - | 1515 | `		 * cast — and rejects a decimal that is not exactly one byte, or an empty` |
|     - | 1516 | `		 * separator set, with a ValueError naming the calling function. */` |
|   136 | 1517 | `		if( pOpts ){` |
|    37 | 1518 | `			ph7_value *pDec = ph7_array_fetch(pOpts,"decimal",(int)sizeof("decimal")-1);` |
|    37 | 1519 | `			ph7_value *pSep = ph7_array_fetch(pOpts,"thousand",(int)sizeof("thousand")-1);` |
|    37 | 1520 | `			if( pDec && ph7_value_is_string(pDec) ){` |
|    13 | 1521 | `				int nDec; const char *zDec = ph7_value_to_string(pDec,&nDec); /* already a string: no conversion */` |
|    13 | 1522 | `				if( nDec!=1 ){` |
|     7 | 1523 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|     2 | 1524 | `						"%s(): \"decimal\" option must be one character long",zFunc);` |
|     - | 1525 | `				}` |
|     9 | 1526 | `				decSep = (unsigned char)zDec[0];` |
|     4 | 1527 | `			}` |
|    33 | 1528 | `			if( pSep && ph7_value_is_string(pSep) ){` |
|    11 | 1529 | `				zSep = ph7_value_to_string(pSep,&nSep);` |
|    11 | 1530 | `				if( nSep<1 ){` |
|     4 | 1531 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|     1 | 1532 | `						"%s(): \"thousand\" option must not be empty",zFunc);` |
|     - | 1533 | `				}` |
|     4 | 1534 | `			}` |
|    15 | 1535 | `		}` |
|   133 | 1536 | `		if( !FvValidateFloat(zVal,nVal,iFlags,decSep,zSep,nSep,&d) ){ goto fail; }` |
|     - | 1537 | `		/* php's range check is the same one the int filter carries, read as a` |
|     - | 1538 | `		 * double (a non-numeric option casts to 0.0, php's own zval_get_double). */` |
|    82 | 1539 | `		if( pOpts ){` |
|    25 | 1540 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|    25 | 1541 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|    25 | 1542 | `			if( pMin && d<PH7_ValuePeekReal(pMin) ){ goto fail; }` |
|    21 | 1543 | `			if( pMax && d>PH7_ValuePeekReal(pMax) ){ goto fail; }` |
|     9 | 1544 | `		}` |
|    76 | 1545 | `		ph7_result_double(pCtx,d);` |
|    76 | 1546 | `		return PH7_OK;` |
|     - | 1547 | `	}` |
|    17 | 1548 | `	case FV_VALIDATE_BOOLEAN: {` |
|     - | 1549 | `		int b;` |
|    37 | 1550 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|    27 | 1551 | `		ph7_result_bool(pCtx,b);` |
|    27 | 1552 | `		return PH7_OK;` |
|     - | 1553 | `	}` |
|   565 | 1554 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|    37 | 1555 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|   204 | 1556 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(pCtx,zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|   146 | 1557 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|   323 | 1558 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     3 | 1559 | `	case FV_VALIDATE_REGEXP: {` |
|     - | 1560 | `#ifdef PH7_ENABLE_PCRE` |
|     8 | 1561 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|     8 | 1562 | `		const char *zRe; int nRe, matched = 0;` |
|     8 | 1563 | `		if( pRe==0 ){` |
|     4 | 1564 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     1 | 1565 | `				"%s(): \"regexp\" option is missing",zFunc);` |
|     - | 1566 | `		}` |
|     5 | 1567 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|     5 | 1568 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|     3 | 1569 | `		goto pass;` |
|     - | 1570 | `#else` |
|     - | 1571 | `		goto fail;` |
|     - | 1572 | `#endif` |
|     - | 1573 | `	}` |
|    26 | 1574 | `	case FV_SANITIZE_STRING:       FvSanitizeStripString(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     9 | 1575 | `	case FV_SANITIZE_ENCODED:      FvSanitizeEncoded(pCtx,zVal,nVal,iFlags);     return PH7_OK;` |
|     7 | 1576 | `	case FV_SANITIZE_ADD_SLASHES:  FvSanitizeAddSlashes(pCtx,zVal,nVal);         return PH7_OK;` |
|     6 | 1577 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|     5 | 1578 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|    16 | 1579 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|    25 | 1580 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|     3 | 1581 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|     3 | 1582 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|    25 | 1583 | `	case FV_DEFAULT:` |
|     - | 1584 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|     - | 1585 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. php` |
|     - | 1586 | `		 * takes that branch only for a NON-empty value, so an empty one falls to` |
|     - | 1587 | `		 * the EMPTY_STRING_NULL rule either way. */` |
|    53 | 1588 | `		if( nVal>0 && (iFlags & FV_FLAG_STRING_MASK) ){` |
|    15 | 1589 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|    15 | 1590 | `			return PH7_OK;` |
|     - | 1591 | `		}` |
|    39 | 1592 | `		if( nVal==0 && (iFlags & FV_FLAG_EMPTY_STRING_NULL) ){` |
|     3 | 1593 | `			ph7_result_null(pCtx);` |
|     3 | 1594 | `			return PH7_OK;` |
|     - | 1595 | `		}` |
|    37 | 1596 | `		goto pass;` |
|     1 | 1597 | `	default:` |
|     - | 1598 | `		/* php's php_zval_filter falls back to the DEFAULT filter for an id it` |
|     - | 1599 | `		 * does not have; the WARNING is raised by the caller that knows which` |
|     - | 1600 | `		 * argument named it. */` |
|     3 | 1601 | `		goto pass;` |
|     - | 1602 | `	}` |
|   359 | 1603 | `fail:` |
|   723 | 1604 | `	if( iFlags & FV_THROW_ON_FAILURE ){` |
|     - | 1605 | `		/* php's message names the FILTER and the value it was given, and the` |
|     - | 1606 | ``		 * `default` option does not suppress it. */`` |
|     - | 1607 | `		ph7_value sTmp;` |
|     - | 1608 | `		int nGiven; const char *zGiven;` |
|    21 | 1609 | `		PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|    21 | 1610 | `		zGiven = ph7_value_to_string(PH7_ValuePeek(pInput,&sTmp),&nGiven);` |
|    21 | 1611 | `		ph7_result_null(pCtx);` |
|    31 | 1612 | `		rc = PH7_VmThrowException(pCtx,"Filter\\FilterFailedException",` |
|     - | 1613 | `			"filter validation failed: filter %s not satisfied by '%.*s'",` |
|    10 | 1614 | `			FvFilterIdName(iFilter),nGiven,zGiven);` |
|    21 | 1615 | `		PH7_MemObjRelease(&sTmp);` |
|    21 | 1616 | `		return rc;` |
|     - | 1617 | `	}` |
|   703 | 1618 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|   693 | 1619 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|   683 | 1620 | `	else { ph7_result_bool(pCtx,0); }` |
|   703 | 1621 | `	return PH7_OK;` |
|   357 | 1622 | `pass: /* validation passed: return the (string) input unchanged */` |
|   718 | 1623 | `	ph7_result_string(pCtx,zVal,nVal);` |
|   718 | 1624 | `	return PH7_OK;` |
|   884 | 1625 | `}` |
|     - | 1626 | `/*` |
|     - | 1627 | ` * php applies the "default" option by looking at what the filter ANSWERED, not` |
|     - | 1628 | ` * at whether it failed: any false (or, under NULL_ON_FAILURE, any null) is` |
|     - | 1629 | `` * replaced. So `['default' => 'D']` replaces the boolean filter's legitimate`` |
|     - | 1630 | ` * FALSE too, which is the one place the two readings differ.` |
|     - | 1631 | ` */` |
|  1758 | 1632 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|     - | 1633 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|     - | 1634 | `                         ph7_value *pDefault,const char *zFunc)` |
|     5 | 1635 | `{` |
|  1763 | 1636 | `	int rc = FvApplyFilterRaw(pCtx,pInput,iFilter,iFlags,pOpts,pDefault,zFunc);` |
|  1763 | 1637 | `	if( rc==PH7_OK && pDefault && pCtx->pRet ){` |
|    19 | 1638 | `		int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|    19 | 1639 | `		ph7_value *pRet = pCtx->pRet;` |
|    28 | 1640 | `		if( bNull ? ph7_value_is_null(pRet)` |
|    16 | 1641 | `		          : (ph7_value_is_bool(pRet) && pRet->x.iVal==0) ){` |
|     3 | 1642 | `			ph7_result_value(pCtx,pDefault);` |
|     1 | 1643 | `		}` |
|    10 | 1644 | `	}` |
|  1765 | 1645 | `	return rc;` |
|     5 | 1646 | `}` |
|     - | 1647 | `/*` |
|     - | 1648 | ` * Filter one value into pOut instead of into the call's own result slot. The` |
|     - | 1649 | ` * filters write through ph7_result_xxx(), so the target is switched by pointing` |
|     - | 1650 | ` * the context's return slot at pOut for the duration.` |
|     - | 1651 | ` */` |
|    86 | 1652 | `static int FvApplyFilterInto(ph7_context *pCtx,ph7_value *pIn,ph7_value *pOut,` |
|     - | 1653 | `                             int iFilter,int iFlags,ph7_value *pOpts,` |
|     - | 1654 | `                             ph7_value *pDefault,const char *zFunc)` |
|     2 | 1655 | `{` |
|    88 | 1656 | `	ph7_value *pSaved = pCtx->pRet;` |
|     - | 1657 | `	int rc;` |
|    88 | 1658 | `	pCtx->pRet = pOut;` |
|    88 | 1659 | `	rc = FvApplyFilter(pCtx,pIn,iFilter,iFlags,pOpts,pDefault,zFunc);` |
|    88 | 1660 | `	pCtx->pRet = pSaved;` |
|    88 | 1661 | `	return rc;` |
|     2 | 1662 | `}` |
|     - | 1663 | `/*` |
|     - | 1664 | ` * php's recursive walk for an ARRAY input: every element is filtered under the` |
|     - | 1665 | ` * same filter and flags, KEYS are kept, and a nested array is walked rather` |
|     - | 1666 | ` * than failed. (php stops at a self-referencing node; the depth cap here is` |
|     - | 1667 | ` * what stands in for its recursion mark.)` |
|     - | 1668 | ` */` |
|     - | 1669 | `typedef struct FvArrayWalk FvArrayWalk;` |
|     - | 1670 | `struct FvArrayWalk {` |
|     - | 1671 | `	ph7_context *pCtx;` |
|     - | 1672 | `	ph7_value *pOut;` |
|     - | 1673 | `	int iFilter, iFlags, iDepth;` |
|     - | 1674 | `	ph7_value *pOpts, *pDefault;` |
|     - | 1675 | `	const char *zFunc;` |
|     - | 1676 | `	int rc;` |
|     - | 1677 | `};` |
|     - | 1678 | `static int FvArrayWalker(ph7_value *pKey,ph7_value *pData,void *pUserData);` |
|    44 | 1679 | `static int FvFilterArrayInto(ph7_context *pCtx,ph7_value *pIn,ph7_value *pOut,` |
|     - | 1680 | `                             int iFilter,int iFlags,ph7_value *pOpts,` |
|     - | 1681 | `                             ph7_value *pDefault,const char *zFunc,int iDepth)` |
|     2 | 1682 | `{` |
|     - | 1683 | `	FvArrayWalk sWalk;` |
|    46 | 1684 | `	sWalk.pCtx = pCtx; sWalk.pOut = pOut;` |
|    46 | 1685 | `	sWalk.iFilter = iFilter; sWalk.iFlags = iFlags; sWalk.iDepth = iDepth;` |
|    46 | 1686 | `	sWalk.pOpts = pOpts; sWalk.pDefault = pDefault; sWalk.zFunc = zFunc;` |
|    46 | 1687 | `	sWalk.rc = PH7_OK;` |
|    46 | 1688 | `	ph7_array_walk(pIn,FvArrayWalker,&sWalk);` |
|    46 | 1689 | `	return sWalk.rc;` |
|     2 | 1690 | `}` |
|     - | 1691 | `/* The walk is C-recursive, as php's is. php stops at a node it has already` |
|     - | 1692 | ` * entered (its recursion mark); this cap is the same idea with a number on it,` |
|     - | 1693 | ` * set to the engine's usual nesting bound so no realistic input reaches it —` |
|     - | 1694 | ` * and a node AT the cap is copied through unfiltered rather than dropped, which` |
|     - | 1695 | ` * is what php does with the node its mark stops at. */` |
|     - | 1696 | `#define FV_MAX_ARRAY_DEPTH 512` |
|    88 | 1697 | `static int FvArrayWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|     2 | 1698 | `{` |
|    90 | 1699 | `	FvArrayWalk *pWalk = (FvArrayWalk *)pUserData;` |
|    90 | 1700 | `	ph7_context *pCtx = pWalk->pCtx;` |
|     - | 1701 | `	ph7_value *pElem;` |
|     - | 1702 | `	int rc;` |
|    90 | 1703 | `	if( ph7_value_is_array(pData) ){` |
|    11 | 1704 | `		if( pWalk->iDepth >= FV_MAX_ARRAY_DEPTH ){` |
|   ! 0 | 1705 | `			ph7_array_add_elem(pWalk->pOut,pKey,pData);` |
|   ! 0 | 1706 | `			return PH7_OK;` |
|     - | 1707 | `		}` |
|    11 | 1708 | `		pElem = ph7_context_new_array(pCtx);` |
|    11 | 1709 | `		if( pElem == 0 ){ return PH7_OK; }` |
|    16 | 1710 | `		rc = FvFilterArrayInto(pCtx,pData,pElem,pWalk->iFilter,pWalk->iFlags,` |
|    10 | 1711 | `		                       pWalk->pOpts,pWalk->pDefault,pWalk->zFunc,pWalk->iDepth+1);` |
|     6 | 1712 | `	}else{` |
|    80 | 1713 | `		pElem = ph7_context_new_scalar(pCtx);` |
|    80 | 1714 | `		if( pElem == 0 ){ return PH7_OK; }` |
|   119 | 1715 | `		rc = FvApplyFilterInto(pCtx,pData,pElem,pWalk->iFilter,pWalk->iFlags,` |
|    39 | 1716 | `		                       pWalk->pOpts,pWalk->pDefault,pWalk->zFunc);` |
|     - | 1717 | `	}` |
|    90 | 1718 | `	ph7_array_add_elem(pWalk->pOut,pKey,pElem);` |
|    90 | 1719 | `	ph7_context_release_value(pCtx,pElem);` |
|    90 | 1720 | `	if( rc != PH7_OK ){` |
|     3 | 1721 | `		pWalk->rc = rc;` |
|     3 | 1722 | `		return SXERR_ABORT; /* a filter threw: stop the walk */` |
|     - | 1723 | `	}` |
|    88 | 1724 | `	return PH7_OK;` |
|    46 | 1725 | `}` |
|     - | 1726 | `/*` |
|     - | 1727 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|     - | 1728 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|     - | 1729 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|     - | 1730 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|     - | 1731 | ` * unset outputs keep the caller-provided defaults.` |
|     - | 1732 | ` *` |
|     - | 1733 | ` * php's own rule for the flags: whichever spelling carries them, a set that` |
|     - | 1734 | ` * names neither REQUIRE_ARRAY nor FORCE_ARRAY gets REQUIRE_SCALAR added -- so` |
|     - | 1735 | ` * "no flags at all" means "an array input is a failure", and asking for one of` |
|     - | 1736 | ` * the array shapes is what turns that off.` |
|     - | 1737 | ` */` |
|  1716 | 1738 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|     - | 1739 | `                              int *piFilter,int *piFlags,` |
|     - | 1740 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|     5 | 1741 | `{` |
|  1721 | 1742 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|  1721 | 1743 | `	if( nArg>iBase+1 ){` |
|  1295 | 1744 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|   134 | 1745 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"filter",(int)sizeof("filter")-1);` |
|   134 | 1746 | `			if( pF ){ *piFilter = (int)PH7_ValuePeekInt64(pF); }` |
|   134 | 1747 | `			pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|   134 | 1748 | `			if( pF ){` |
|    60 | 1749 | `				*piFlags = (int)PH7_ValuePeekInt64(pF);` |
|    60 | 1750 | `				if( (*piFlags & (FV_REQUIRE_ARRAY\|FV_FORCE_ARRAY))==0 ){ *piFlags \|= FV_REQUIRE_SCALAR; }` |
|    28 | 1751 | `			}` |
|   134 | 1752 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|   134 | 1753 | `			if( *ppOpts && *piFilter == FV_CALLBACK ){` |
|     - | 1754 | `				/* the CALLBACK filter's "options" IS the callable, and php clears` |
|     - | 1755 | `				 * the shape flags for it */` |
|    23 | 1756 | `				*piFlags = 0;` |
|    12 | 1757 | `			}else{` |
|   112 | 1758 | `				if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|   112 | 1759 | `				if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     - | 1760 | `			}` |
|    69 | 1761 | `		}else{` |
|  1165 | 1762 | `			*piFlags = (int)PH7_ValuePeekInt64(apArg[iBase+1]);` |
|  1165 | 1763 | `			if( (*piFlags & (FV_REQUIRE_ARRAY\|FV_FORCE_ARRAY))==0 ){ *piFlags \|= FV_REQUIRE_SCALAR; }` |
|     - | 1764 | `		}` |
|   645 | 1765 | `	}` |
|  1721 | 1766 | `}` |
|     - | 1767 | `/*` |
|     - | 1768 | ` * php refuses the two failure modes together, and words it against the argument` |
|     - | 1769 | ` * that carried them.` |
|     - | 1770 | ` */` |
|  1716 | 1771 | `static int FvCheckFailureFlags(ph7_context *pCtx,int iFlags,const char *zFunc,int iArgNo,` |
|     - | 1772 | `                               const char *zArgName)` |
|     5 | 1773 | `{` |
|  1721 | 1774 | `	if( (iFlags & FV_NULL_ON_FAILURE) && (iFlags & FV_THROW_ON_FAILURE) ){` |
|     3 | 1775 | `		ph7_result_null(pCtx);` |
|     4 | 1776 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1777 | `			"%s(): Argument #%d ($%s) cannot use both FILTER_NULL_ON_FAILURE and FILTER_THROW_ON_FAILURE",` |
|     1 | 1778 | `			zFunc,iArgNo,zArgName);` |
|     - | 1779 | `	}` |
|  1719 | 1780 | `	return PH7_OK;` |
|   863 | 1781 | `}` |
|     - | 1782 | `/*` |
|     - | 1783 | `` * php's `array\|int $options`: an array or an int, and nothing else. The shared`` |
|     - | 1784 | `` * type screen leaves a union with an `array` arm alone (php words those from`` |
|     - | 1785 | ` * the builtin's own check), so this is that check.` |
|     - | 1786 | ` */` |
|  1328 | 1787 | `static int FvCheckOptionsArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNo,` |
|     - | 1788 | `                             const char *zArgName)` |
|     5 | 1789 | `{` |
|  1328 | 1790 | `	if( pArg==0 \|\| ph7_value_is_array(pArg) \|\| ph7_value_is_int(pArg)` |
|   597 | 1791 | `	 \|\| ph7_value_is_bool(pArg) \|\| ph7_value_is_null(pArg) ){` |
|  1323 | 1792 | `		return PH7_OK;` |
|     - | 1793 | `	}` |
|    11 | 1794 | `	if( ph7_value_is_float(pArg) \|\| PH7_MemObjStringIsNumeric(pArg) ){` |
|     3 | 1795 | `		return PH7_OK; /* weak mode coerces a number the way php's ZPP does */` |
|     - | 1796 | `	}` |
|     9 | 1797 | `	ph7_result_null(pCtx);` |
|    13 | 1798 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1799 | `		"%s(): Argument #%d ($%s) must be of type array\|int, %s given",` |
|     4 | 1800 | `		zFunc,iArgNo,zArgName,ph7_type_name(pArg));` |
|   669 | 1801 | `}` |
|     - | 1802 | `/*` |
|     - | 1803 | ` * php's php_filter_call: what the ARRAY shape flags decide before any filter` |
|     - | 1804 | ` * runs. An array input is refused under REQUIRE_SCALAR and walked otherwise; a` |
|     - | 1805 | ` * scalar input is refused under REQUIRE_ARRAY; and FORCE_ARRAY wraps whatever` |
|     - | 1806 | ` * the filter answered in a one-element list.` |
|     - | 1807 | ` */` |
|  1728 | 1808 | `static int FvFilterCall(ph7_context *pCtx,ph7_value *pInput,` |
|     - | 1809 | `                        int iFilter,int iFlags,ph7_value *pOpts,` |
|     - | 1810 | `                        ph7_value *pDefault,const char *zFunc)` |
|     5 | 1811 | `{` |
|  1733 | 1812 | `	if( ph7_value_is_array(pInput) ){` |
|     - | 1813 | `		ph7_value *pOut;` |
|     - | 1814 | `		int rc;` |
|    47 | 1815 | `		if( iFlags & FV_REQUIRE_SCALAR ){` |
|    12 | 1816 | `			if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_null(pCtx); }` |
|    10 | 1817 | `			else{ ph7_result_bool(pCtx,0); }` |
|    12 | 1818 | `			return PH7_OK;` |
|     - | 1819 | `		}` |
|    36 | 1820 | `		pOut = ph7_context_new_array(pCtx);` |
|    36 | 1821 | `		if( pOut == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|    36 | 1822 | `		rc = FvFilterArrayInto(pCtx,pInput,pOut,iFilter,iFlags,pOpts,pDefault,zFunc,0);` |
|    36 | 1823 | `		if( rc == PH7_OK ){ ph7_result_value(pCtx,pOut); }` |
|    36 | 1824 | `		ph7_context_release_value(pCtx,pOut);` |
|    36 | 1825 | `		return rc;` |
|     - | 1826 | `	}` |
|  1689 | 1827 | `	if( iFlags & FV_REQUIRE_ARRAY ){` |
|     5 | 1828 | `		if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_null(pCtx); }` |
|     3 | 1829 | `		else{ ph7_result_bool(pCtx,0); }` |
|     5 | 1830 | `		return PH7_OK;` |
|     - | 1831 | `	}` |
|  1685 | 1832 | `	if( iFlags & FV_FORCE_ARRAY ){` |
|     9 | 1833 | `		ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     9 | 1834 | `		ph7_value *pElem = ph7_context_new_scalar(pCtx);` |
|     - | 1835 | `		int rc;` |
|     9 | 1836 | `		if( pOut == 0 \|\| pElem == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|     9 | 1837 | `		rc = FvApplyFilterInto(pCtx,pInput,pElem,iFilter,iFlags,pOpts,pDefault,zFunc);` |
|     9 | 1838 | `		if( rc == PH7_OK ){` |
|     9 | 1839 | `			ph7_array_add_elem(pOut,0,pElem);` |
|     9 | 1840 | `			ph7_result_value(pCtx,pOut);` |
|     4 | 1841 | `		}` |
|     9 | 1842 | `		ph7_context_release_value(pCtx,pElem);` |
|     9 | 1843 | `		ph7_context_release_value(pCtx,pOut);` |
|     9 | 1844 | `		return rc;` |
|     - | 1845 | `	}` |
|  1677 | 1846 | `	return FvApplyFilter(pCtx,pInput,iFilter,iFlags,pOpts,pDefault,zFunc);` |
|   869 | 1847 | `}` |
|     - | 1848 | `/*` |
|     - | 1849 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|     - | 1850 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|     - | 1851 | ` */` |
|  1698 | 1852 | `PH7_PRIVATE int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 1853 | `{` |
|  1703 | 1854 | `	int iFilter = FV_DEFAULT, iFlags = FV_REQUIRE_SCALAR;` |
|  1703 | 1855 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|  1703 | 1856 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|  1703 | 1857 | `	if( nArg>2 && FvCheckOptionsArg(pCtx,apArg[2],"filter_var",3,"options")!=PH7_OK ){` |
|     5 | 1858 | `		return PH7_EXCEPTION;` |
|     - | 1859 | `	}` |
|  1699 | 1860 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|  1699 | 1861 | `	if( FvCheckFailureFlags(pCtx,iFlags,"filter_var",3,"options")!=PH7_OK ){` |
|     3 | 1862 | `		return PH7_EXCEPTION;` |
|     - | 1863 | `	}` |
|  1697 | 1864 | `	if( !FvFilterIdExists(iFilter) ){` |
|     3 | 1865 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown filter with ID %d",iFilter);` |
|     3 | 1866 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1867 | `		return PH7_OK;` |
|     - | 1868 | `	}` |
|  1695 | 1869 | `	return FvFilterCall(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault,"filter_var");` |
|   854 | 1870 | `}` |
|     - | 1871 | `/*` |
|     - | 1872 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|     - | 1873 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|     - | 1874 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|     - | 1875 | ` *   - variable NOT set: 'default' option wins, else false when` |
|     - | 1876 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|     - | 1877 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|     - | 1878 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|     - | 1879 | ` *   - variable present: delegate to FvApplyFilter.` |
|     - | 1880 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|     - | 1881 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|     - | 1882 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|     - | 1883 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|     - | 1884 | ` *  php's snapshot.` |
|     - | 1885 | ` */` |
|    24 | 1886 | `PH7_PRIVATE int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1887 | `{` |
|    26 | 1888 | `	int iType, iFilter = FV_DEFAULT, iFlags = FV_REQUIRE_SCALAR;` |
|    26 | 1889 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|     - | 1890 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|    26 | 1891 | `	if( nArg<2 ){` |
|   ! 0 | 1892 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|   ! 0 | 1893 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|     - | 1894 | `	}` |
|    26 | 1895 | `	iType = ph7_value_to_int(apArg[0]);` |
|    26 | 1896 | `	switch( iType ){` |
|     3 | 1897 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|     3 | 1898 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|   ! 0 | 1899 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|   ! 0 | 1900 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|    19 | 1901 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|     1 | 1902 | `	default:` |
|     3 | 1903 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1904 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|     - | 1905 | `	}` |
|    23 | 1906 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|    23 | 1907 | `	if( nArg>3 && FvCheckOptionsArg(pCtx,apArg[3],"filter_input",4,"options")!=PH7_OK ){` |
|   ! 0 | 1908 | `		return PH7_EXCEPTION;` |
|     - | 1909 | `	}` |
|    23 | 1910 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    23 | 1911 | `	if( FvCheckFailureFlags(pCtx,iFlags,"filter_input",4,"options")!=PH7_OK ){` |
|   ! 0 | 1912 | `		return PH7_EXCEPTION;` |
|     - | 1913 | `	}` |
|    23 | 1914 | `	if( !FvFilterIdExists(iFilter) ){` |
|   ! 0 | 1915 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown filter with ID %d",iFilter);` |
|   ! 0 | 1916 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1917 | `		return PH7_OK;` |
|     - | 1918 | `	}` |
|     - | 1919 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|    23 | 1920 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|    23 | 1921 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|    33 | 1922 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|    23 | 1923 | `	if( pElem==0 ){` |
|     - | 1924 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|     - | 1925 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|    13 | 1926 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|     9 | 1927 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|     7 | 1928 | `		else { ph7_result_null(pCtx); }` |
|    13 | 1929 | `		return PH7_OK;` |
|     - | 1930 | `	}` |
|    11 | 1931 | `	return FvFilterCall(pCtx,pElem,iFilter,iFlags,pOpts,pDefault,"filter_input");` |
|    14 | 1932 | `}` |
|     - | 1933 | `/*` |
|     - | 1934 | ` * filter_list(): every filter NAME php knows, in php's order.` |
|     - | 1935 | ` */` |
|     4 | 1936 | `PH7_PRIVATE int PH7_builtin_filter_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1937 | `{` |
|     - | 1938 | `	ph7_value *pArray, *pVal;` |
|     - | 1939 | `	sxu32 n;` |
|     2 | 1940 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 1941 | `	pArray = ph7_context_new_array(pCtx);` |
|     5 | 1942 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     5 | 1943 | `	if( pArray==0 \|\| pVal==0 ){` |
|   ! 0 | 1944 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1945 | `		return PH7_OK;` |
|     - | 1946 | `	}` |
|    89 | 1947 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFvFilterName) ; ++n ){` |
|    85 | 1948 | `		ph7_value_string(pVal,aFvFilterName[n].zName,-1);` |
|    85 | 1949 | `		ph7_array_add_elem(pArray,0,pVal);` |
|    85 | 1950 | `		ph7_value_reset_string_cursor(pVal);` |
|    43 | 1951 | `	}` |
|     5 | 1952 | `	ph7_result_value(pCtx,pArray);` |
|     5 | 1953 | `	ph7_context_release_value(pCtx,pVal);` |
|     5 | 1954 | `	ph7_context_release_value(pCtx,pArray);` |
|     5 | 1955 | `	return PH7_OK;` |
|     3 | 1956 | `}` |
|     - | 1957 | `/*` |
|     - | 1958 | ` * filter_id(string $name): the id behind a filter name, or false. The lookup is` |
|     - | 1959 | ` * php's: exact, case-SENSITIVE, over the same table filter_list() answers.` |
|     - | 1960 | ` */` |
|    48 | 1961 | `PH7_PRIVATE int PH7_builtin_filter_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1962 | `{` |
|     - | 1963 | `	const char *zName; int nName; sxu32 n;` |
|    49 | 1964 | `	if( nArg<1 ){` |
|   ! 0 | 1965 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|   ! 0 | 1966 | `			"filter_id() expects exactly 1 argument, %d given",nArg);` |
|     - | 1967 | `	}` |
|    49 | 1968 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|   595 | 1969 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFvFilterName) ; ++n ){` |
|   588 | 1970 | `		if( (int)SyStrlen(aFvFilterName[n].zName)==nName` |
|   329 | 1971 | `		 && SyMemcmp(aFvFilterName[n].zName,zName,(sxu32)nName)==0 ){` |
|    43 | 1972 | `			ph7_result_int(pCtx,aFvFilterName[n].iId);` |
|    43 | 1973 | `			return PH7_OK;` |
|     - | 1974 | `		}` |
|   274 | 1975 | `	}` |
|     7 | 1976 | `	ph7_result_bool(pCtx,0);` |
|     7 | 1977 | `	return PH7_OK;` |
|    25 | 1978 | `}` |
|     - | 1979 | `/*` |
|     - | 1980 | ` * filter_has_var(int $input_type, string $var_name): is the variable there at` |
|     - | 1981 | ` * all, before any filter runs. (Same divergence filter_input() records: php` |
|     - | 1982 | ` * reads the SAPI's startup snapshot of the request variables, PHL the live` |
|     - | 1983 | ` * superglobal.)` |
|     - | 1984 | ` */` |
|     4 | 1985 | `PH7_PRIVATE int PH7_builtin_filter_has_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1986 | `{` |
|     - | 1987 | `	const char *zSuper, *zVar; sxu32 nSuper; int nVar;` |
|     - | 1988 | `	ph7_value *pSuper;` |
|     5 | 1989 | `	if( nArg<2 ){` |
|   ! 0 | 1990 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|   ! 0 | 1991 | `			"filter_has_var() expects exactly 2 arguments, %d given",nArg);` |
|     - | 1992 | `	}` |
|     5 | 1993 | `	zSuper = FvInputSuper(ph7_value_to_int(apArg[0]),&nSuper);` |
|     5 | 1994 | `	if( zSuper==0 ){` |
|     3 | 1995 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1996 | `			"filter_has_var(): Argument #1 ($input_type) must be an INPUT_* constant");` |
|     - | 1997 | `	}` |
|     3 | 1998 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     3 | 1999 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     5 | 2000 | `	ph7_result_bool(pCtx,(pSuper && ph7_value_is_array(pSuper)` |
|     2 | 2001 | `	                      && ph7_array_fetch(pSuper,zVar,nVar)) ? 1 : 0);` |
|     3 | 2002 | `	return PH7_OK;` |
|     3 | 2003 | `}` |
|     - | 2004 | `/*` |
|     - | 2005 | ` * The body php's filter_var_array()/filter_input_array() share.` |
|     - | 2006 | ` *` |
|     - | 2007 | ` * A plain int $options is one filter for the WHOLE array (php defaults the` |
|     - | 2008 | ` * shape flag to REQUIRE_ARRAY there, so the array is walked rather than` |
|     - | 2009 | ` * refused). A definition ARRAY is per-key: its keys name the entries to read,` |
|     - | 2010 | ` * and each value is either a filter id or the same {filter, flags, options}` |
|     - | 2011 | ` * array filter_var() takes. A key that is not in the input answers NULL when` |
|     - | 2012 | ` * $add_empty is on, and is left out otherwise.` |
|     - | 2013 | ` */` |
|    32 | 2014 | `static int FvArrayHandler(ph7_context *pCtx,ph7_value *pInput,int nArg,ph7_value **apArg,` |
|     - | 2015 | `                          int iSpecArg,const char *zFunc)` |
|     2 | 2016 | `{` |
|    34 | 2017 | `	int bAddEmpty = 1;` |
|    34 | 2018 | `	ph7_value *pSpec = (nArg>iSpecArg) ? apArg[iSpecArg] : 0;` |
|    34 | 2019 | `	if( pSpec && FvCheckOptionsArg(pCtx,pSpec,zFunc,iSpecArg+1,"options")!=PH7_OK ){` |
|     3 | 2020 | `		return PH7_EXCEPTION;` |
|     - | 2021 | `	}` |
|    31 | 2022 | `	if( nArg>iSpecArg+1 ){ bAddEmpty = PH7_ValuePeekBool(apArg[iSpecArg+1]); }` |
|    31 | 2023 | `	if( pSpec==0 \|\| !ph7_value_is_array(pSpec) ){` |
|     - | 2024 | `		/* one filter over the whole array */` |
|     9 | 2025 | `		int iFilter = pSpec ? (int)PH7_ValuePeekInt64(pSpec) : FV_DEFAULT;` |
|     9 | 2026 | `		if( !FvFilterIdExists(iFilter) ){` |
|   ! 0 | 2027 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown filter with ID %d",iFilter);` |
|   ! 0 | 2028 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 | 2029 | `			return PH7_OK;` |
|     - | 2030 | `		}` |
|     9 | 2031 | `		return FvFilterCall(pCtx,pInput,iFilter,FV_REQUIRE_ARRAY,0,0,zFunc);` |
|   ! 0 | 2032 | `	}else{` |
|    23 | 2033 | `		ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     - | 2034 | `		ph7_hashmap *pMap;` |
|     - | 2035 | `		ph7_hashmap_node *pNode;` |
|     - | 2036 | `		sxu32 n;` |
|    23 | 2037 | `		if( pOut==0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|    23 | 2038 | `		pMap = (ph7_hashmap *)pSpec->x.pOther;` |
|    23 | 2039 | `		pNode = pMap->pFirst;` |
|    47 | 2040 | `		for( n = 0 ; n < pMap->nEntry && pNode ; ++n, pNode = pNode->pPrev ){` |
|     - | 2041 | `			ph7_value sKey, sElem, *pSrc, *pRes;` |
|     - | 2042 | `			const char *zKey; int nKey;` |
|    29 | 2043 | `			int iFilter = FV_DEFAULT, iFlags = FV_REQUIRE_SCALAR;` |
|    29 | 2044 | `			ph7_value *pOpts = 0, *pDefault = 0;` |
|    29 | 2045 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|    29 | 2046 | `			PH7_MemObjInit(pCtx->pVm,&sElem);` |
|    29 | 2047 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|    29 | 2048 | `			PH7_HashmapExtractNodeValue(pNode,&sElem,FALSE);` |
|    29 | 2049 | `			if( !ph7_value_is_string(&sKey) ){` |
|     3 | 2050 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sElem);` |
|     3 | 2051 | `				ph7_context_release_value(pCtx,pOut);` |
|     5 | 2052 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|     1 | 2053 | `					"%s(): Argument #2 ($options) must contain only string keys",zFunc);` |
|     - | 2054 | `			}` |
|    27 | 2055 | `			zKey = ph7_value_to_string(&sKey,&nKey);` |
|    27 | 2056 | `			if( nKey<1 ){` |
|     3 | 2057 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sElem);` |
|     3 | 2058 | `				ph7_context_release_value(pCtx,pOut);` |
|     4 | 2059 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|     1 | 2060 | `					"%s(): Argument #2 ($options) cannot contain empty keys",zFunc);` |
|     - | 2061 | `			}` |
|     - | 2062 | `			/* the per-key spelling: a bare filter id, or filter_var()'s own` |
|     - | 2063 | `			 * {filter, flags, options} array */` |
|    25 | 2064 | `			if( ph7_value_is_array(&sElem) ){` |
|    11 | 2065 | `				ph7_value *pF = ph7_array_fetch(&sElem,"filter",(int)sizeof("filter")-1);` |
|    11 | 2066 | `				if( pF ){ iFilter = (int)PH7_ValuePeekInt64(pF); }` |
|    11 | 2067 | `				pF = ph7_array_fetch(&sElem,"flags",(int)sizeof("flags")-1);` |
|    11 | 2068 | `				if( pF ){` |
|     7 | 2069 | `					iFlags = (int)PH7_ValuePeekInt64(pF);` |
|     7 | 2070 | `					if( (iFlags & (FV_REQUIRE_ARRAY\|FV_FORCE_ARRAY))==0 ){ iFlags \|= FV_REQUIRE_SCALAR; }` |
|     3 | 2071 | `				}` |
|    11 | 2072 | `				pOpts = ph7_array_fetch(&sElem,"options",(int)sizeof("options")-1);` |
|    11 | 2073 | `				if( pOpts && iFilter == FV_CALLBACK ){` |
|   ! 0 | 2074 | `					iFlags = 0; /* the callable is the option; php clears the shape flags */` |
|   ! 0 | 2075 | `				}else{` |
|    11 | 2076 | `					if( pOpts && !ph7_value_is_array(pOpts) ){ pOpts = 0; }` |
|    11 | 2077 | `					if( pOpts ){ pDefault = ph7_array_fetch(pOpts,"default",(int)sizeof("default")-1); }` |
|     - | 2078 | `				}` |
|     6 | 2079 | `			}else{` |
|    15 | 2080 | `				iFilter = (int)PH7_ValuePeekInt64(&sElem);` |
|    15 | 2081 | `				if( !FvFilterIdExists(iFilter) ){` |
|     4 | 2082 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     1 | 2083 | `						"Unknown filter with ID %d",iFilter);` |
|     1 | 2084 | `				}` |
|     - | 2085 | `			}` |
|    25 | 2086 | `			pSrc = (pInput && ph7_value_is_array(pInput)) ? ph7_array_fetch(pInput,zKey,nKey) : 0;` |
|    25 | 2087 | `			pRes = ph7_context_new_scalar(pCtx);` |
|    25 | 2088 | `			if( pRes==0 ){ PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sElem); break; }` |
|    25 | 2089 | `			if( pSrc==0 ){` |
|     5 | 2090 | `				if( bAddEmpty ){` |
|     3 | 2091 | `					ph7_value_null(pRes);` |
|     3 | 2092 | `					ph7_array_add_elem(pOut,&sKey,pRes);` |
|     1 | 2093 | `				}` |
|     3 | 2094 | `			}else{` |
|    21 | 2095 | `				ph7_value *pSaved = pCtx->pRet;` |
|     - | 2096 | `				int rc;` |
|    21 | 2097 | `				pCtx->pRet = pRes;` |
|    21 | 2098 | `				rc = FvFilterCall(pCtx,pSrc,iFilter,iFlags,pOpts,pDefault,zFunc);` |
|    21 | 2099 | `				pCtx->pRet = pSaved;` |
|    21 | 2100 | `				if( rc != PH7_OK ){` |
|   ! 0 | 2101 | `					ph7_context_release_value(pCtx,pRes);` |
|   ! 0 | 2102 | `					PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sElem);` |
|   ! 0 | 2103 | `					ph7_context_release_value(pCtx,pOut);` |
|   ! 0 | 2104 | `					return rc;` |
|     - | 2105 | `				}` |
|    21 | 2106 | `				ph7_array_add_elem(pOut,&sKey,pRes);` |
|     - | 2107 | `			}` |
|    25 | 2108 | `			ph7_context_release_value(pCtx,pRes);` |
|    25 | 2109 | `			PH7_MemObjRelease(&sKey);` |
|    25 | 2110 | `			PH7_MemObjRelease(&sElem);` |
|    13 | 2111 | `		}` |
|    19 | 2112 | `		ph7_result_value(pCtx,pOut);` |
|    19 | 2113 | `		ph7_context_release_value(pCtx,pOut);` |
|    19 | 2114 | `		return PH7_OK;` |
|     - | 2115 | `	}` |
|    18 | 2116 | `}` |
|     - | 2117 | `/*` |
|     - | 2118 | ` * filter_var_array($array, $options = FILTER_DEFAULT, $add_empty = true)` |
|     - | 2119 | ` */` |
|    32 | 2120 | `PH7_PRIVATE int PH7_builtin_filter_var_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2121 | `{` |
|    34 | 2122 | `	if( nArg<1 ){` |
|   ! 0 | 2123 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|   ! 0 | 2124 | `			"filter_var_array() expects at least 1 argument, %d given",nArg);` |
|     - | 2125 | `	}` |
|    34 | 2126 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|   ! 0 | 2127 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 2128 | `			"filter_var_array(): Argument #1 ($array) must be of type array, %s given",` |
|   ! 0 | 2129 | `			ph7_type_name(apArg[0]));` |
|     - | 2130 | `	}` |
|    34 | 2131 | `	return FvArrayHandler(pCtx,apArg[0],nArg,apArg,1,"filter_var_array");` |
|    18 | 2132 | `}` |
|     - | 2133 | `/*` |
|     - | 2134 | ` * filter_input_array($type, $options = FILTER_DEFAULT, $add_empty = true)` |
|     - | 2135 | ` *  The same handler over an INPUT_* superglobal; a source with no data at all` |
|     - | 2136 | ` *  is php's NULL rather than an empty array.` |
|     - | 2137 | ` */` |
|     6 | 2138 | `PH7_PRIVATE int PH7_builtin_filter_input_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2139 | `{` |
|     - | 2140 | `	const char *zSuper; sxu32 nSuper;` |
|     - | 2141 | `	ph7_value *pSuper;` |
|     8 | 2142 | `	if( nArg<1 ){` |
|   ! 0 | 2143 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|   ! 0 | 2144 | `			"filter_input_array() expects at least 1 argument, %d given",nArg);` |
|     - | 2145 | `	}` |
|     - | 2146 | `	/* php's ZPP screens the arguments before it looks at the source at all. */` |
|     8 | 2147 | `	if( nArg>1 && FvCheckOptionsArg(pCtx,apArg[1],"filter_input_array",2,"options")!=PH7_OK ){` |
|     3 | 2148 | `		return PH7_EXCEPTION;` |
|     - | 2149 | `	}` |
|     5 | 2150 | `	zSuper = FvInputSuper(ph7_value_to_int(apArg[0]),&nSuper);` |
|     5 | 2151 | `	if( zSuper==0 ){` |
|     3 | 2152 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 2153 | `			"filter_input_array(): Argument #1 ($type) must be an INPUT_* constant");` |
|     - | 2154 | `	}` |
|     3 | 2155 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     3 | 2156 | `	if( pSuper==0 \|\| !ph7_value_is_array(pSuper) \|\| ph7_array_count(pSuper)<1 ){` |
|     3 | 2157 | `		ph7_result_null(pCtx);` |
|     3 | 2158 | `		return PH7_OK;` |
|     - | 2159 | `	}` |
|   ! 0 | 2160 | `	return FvArrayHandler(pCtx,pSuper,nArg,apArg,1,"filter_input_array");` |
|     5 | 2161 | `}` |
|     - | 2162 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|     - | 2163 | `#ifdef PH7_NEED_FMT_AND_INI` |
|     - | 2164 | `/*` |
|     - | 2165 | ` * The incremental half of the parser below: walk a partially-read record and` |
|     - | 2166 | ` * answer whether it ends INSIDE an enclosure, which is how fgetcsv() knows the` |
|     - | 2167 | ` * value contains the newline and the record continues on the next line.` |
|     - | 2168 | ` *` |
|     - | 2169 | ` * It carries its position and state across appended chunks on purpose. Deciding` |
|     - | 2170 | ` * it by re-parsing the whole accumulated record after every line is quadratic,` |
|     - | 2171 | ` * and one stray quote in a large file is exactly the input that triggers it.` |
|     - | 2172 | ` * The trailing line ending PH7_ProcessCsv strips cannot close an enclosure, so` |
|     - | 2173 | ` * this scan ignores it and both agree on every record.` |
|     - | 2174 | ` */` |
|    44 | 2175 | `PH7_PRIVATE void PH7_CsvScanInit(PH7_CsvScan *pScan)` |
|     1 | 2176 | `{` |
|    45 | 2177 | `	pScan->iState = 0;` |
|    45 | 2178 | `	pScan->nPos = 0;` |
|    45 | 2179 | `}` |
|    54 | 2180 | `PH7_PRIVATE int PH7_CsvScanOpen(PH7_CsvScan *pScan,const char *zIn,sxu32 nByte,` |
|     - | 2181 | `	int delim,int encl,int escape)` |
|     1 | 2182 | `{` |
|    55 | 2183 | `	sxu32 i = pScan->nPos;` |
|   373 | 2184 | `	while( i < nByte ){` |
|   319 | 2185 | `		int c = (unsigned char)zIn[i];` |
|   319 | 2186 | `		switch( pScan->iState ){` |
|    43 | 2187 | `		case 0: /* at a field start: whitespace only counts as padding when an` |
|     - | 2188 | `		         * enclosure is what it leads to */` |
|    87 | 2189 | `			if( c != delim && SyisSpace(c) ){` |
|     7 | 2190 | `				i++;` |
|     7 | 2191 | `				continue;` |
|     - | 2192 | `			}` |
|    81 | 2193 | `			if( c == encl ){` |
|    21 | 2194 | `				pScan->iState = 2;` |
|    21 | 2195 | `				i++;` |
|    21 | 2196 | `				continue;` |
|     - | 2197 | `			}` |
|    61 | 2198 | `			if( c == delim ){` |
|     5 | 2199 | `				i++;` |
|     5 | 2200 | `				continue;` |
|     - | 2201 | `			}` |
|    57 | 2202 | `			pScan->iState = 1;` |
|    57 | 2203 | `			continue;` |
|    56 | 2204 | `		case 1: /* unquoted field: an enclosure here is ordinary content */` |
|   113 | 2205 | `			if( c == delim ){` |
|    25 | 2206 | `				pScan->iState = 0;` |
|    12 | 2207 | `			}` |
|   113 | 2208 | `			i++;` |
|   113 | 2209 | `			continue;` |
|    51 | 2210 | `		case 2: /* inside the enclosure */` |
|   103 | 2211 | `			if( c == encl ){` |
|    21 | 2212 | `				if( i + 1 < nByte && (unsigned char)zIn[i+1] == encl ){` |
|     3 | 2213 | `					i += 2;   /* doubled: a literal enclosure */` |
|     3 | 2214 | `					continue;` |
|     - | 2215 | `				}` |
|    19 | 2216 | `				pScan->iState = 3;` |
|    19 | 2217 | `				i++;` |
|    19 | 2218 | `				continue;` |
|     - | 2219 | `			}` |
|    83 | 2220 | `			if( escape != PH7_CSV_NO_ESCAPE && c == escape ){` |
|   ! 0 | 2221 | `				i += (i + 1 < nByte) ? 2 : 1;` |
|   ! 0 | 2222 | `				continue;` |
|     - | 2223 | `			}` |
|    83 | 2224 | `			i++;` |
|    83 | 2225 | `			continue;` |
|     9 | 2226 | `		default: /* past the closing enclosure, up to the delimiter */` |
|    19 | 2227 | `			if( c == delim ){` |
|    15 | 2228 | `				pScan->iState = 0;` |
|     7 | 2229 | `			}` |
|    19 | 2230 | `			i++;` |
|    19 | 2231 | `			continue;` |
|     - | 2232 | `		}` |
|   ! 0 | 2233 | `	}` |
|    55 | 2234 | `	pScan->nPos = i;` |
|    55 | 2235 | `	return pScan->iState == 2;` |
|     1 | 2236 | `}` |
|     - | 2237 | `/*` |
|     - | 2238 | ` * Strip ONE trailing line ending -- "\r\n", "\n" or "\r" -- and answer the` |
|     - | 2239 | ` * length left. php applies it to the whole line before parsing, and again to` |
|     - | 2240 | ` * each UNQUOTED field's own content (which is how a lone "\n" reads back as the` |
|     - | 2241 | ` * empty string while "a\r\rb" keeps both of its carriage returns).` |
|     - | 2242 | ` */` |
|   312 | 2243 | `static int CsvStripEol(const char *zIn,int nByte)` |
|     1 | 2244 | `{` |
|   313 | 2245 | `	if( nByte > 0 && zIn[nByte-1] == '\n' ){` |
|    59 | 2246 | `		nByte--;` |
|    59 | 2247 | `		if( nByte > 0 && zIn[nByte-1] == '\r' ){` |
|     9 | 2248 | `			nByte--;` |
|     5 | 2249 | `		}` |
|   284 | 2250 | `	}else if( nByte > 0 && zIn[nByte-1] == '\r' ){` |
|     5 | 2251 | `		nByte--;` |
|     2 | 2252 | `	}` |
|   313 | 2253 | `	return nByte;` |
|     1 | 2254 | `}` |
|     - | 2255 | `/*` |
|     - | 2256 | ` * Parse one CSV record and append each field to pArray.` |
|     - | 2257 | ` *` |
|     - | 2258 | ` * A port of php's php_fgetcsv rules, derived from the oracle field by field.` |
|     - | 2259 | ` * PH7's tokenizer answered a different record for most inputs that were not` |
|     - | 2260 | ` * already trivial:` |
|     - | 2261 | `` *  - an EMPTY field was dropped along with its delimiter, so `,a` read as one`` |
|     - | 2262 | `` *    column and `a,b,` as two -- every later column shifted;`` |
|     - | 2263 | ` *  - a doubled enclosure was not undoubled, and the closing one was located by` |
|     - | 2264 | `` *    a parity toggle, so `"a""b"` came back with its quoting intact;`` |
|     - | 2265 | ` *  - the field content was TRIMMED of whitespace and NUL bytes by the consumer,` |
|     - | 2266 | `` *    so `" a "` -- quoted precisely to keep those spaces -- lost them;`` |
|     - | 2267 | ` *  - the escape character consumed the byte after it even OUTSIDE an enclosure,` |
|     - | 2268 | ` *    where php gives it no meaning at all.` |
|     - | 2269 | ` * The rules that are not guessable are the ones php's own parser reaches by` |
|     - | 2270 | ` * accident and programs depend on: whitespace before an opening enclosure is` |
|     - | 2271 | ` * skipped (but kept when no enclosure follows), whatever trails a CLOSING` |
|     - | 2272 | `` * enclosure up to the delimiter is APPENDED to the field (`"a"b` is `ab`), an`` |
|     - | 2273 | ` * escape keeps BOTH bytes rather than the escaped one alone, and a record whose` |
|     - | 2274 | ` * whole text is empty is a single NULL field rather than an empty string.` |
|     - | 2275 | ` *` |
|     - | 2276 | ` * *pbOpen (optional) reports that the text ran out inside an enclosure, which is` |
|     - | 2277 | ` * how fgetcsv() knows a quoted newline means the record continues on the next` |
|     - | 2278 | ` * line.` |
|     - | 2279 | ` */` |
|   154 | 2280 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|     - | 2281 | `	ph7_value *pArray, /* Fields are appended here */` |
|     - | 2282 | `	const char *zInput, /* Raw input */` |
|     - | 2283 | `	int nByte,  /* Input length */` |
|     - | 2284 | `	int delim,  /* Delimiter */` |
|     - | 2285 | `	int encl,   /* Enclosure */` |
|     - | 2286 | `	int escape, /* Escape character, or PH7_CSV_NO_ESCAPE */` |
|     - | 2287 | `	int *pbOpen /* OUT: the text ended inside an enclosure */` |
|     - | 2288 | `	)` |
|     1 | 2289 | `{` |
|   155 | 2290 | `	ph7_vm *pVm = pArray->pVm;` |
|     - | 2291 | `	SyBlob sField;` |
|     - | 2292 | `	ph7_value sEntry;` |
|   155 | 2293 | `	int nLimit = CsvStripEol(zInput,nByte);` |
|   155 | 2294 | `	int i = 0;` |
|   155 | 2295 | `	int bFirst = 1;` |
|   155 | 2296 | `	if( pbOpen ){` |
|   ! 0 | 2297 | `		*pbOpen = 0;` |
|   ! 0 | 2298 | `	}` |
|   155 | 2299 | `	SyBlobInit(&sField,&pVm->sAllocator);` |
|   126 | 2300 | `	for(;;){` |
|     - | 2301 | `		int bQuoted;` |
|   253 | 2302 | `		SyBlobReset(&sField);` |
|     - | 2303 | `		/* Whitespace in front of an OPENING enclosure is not part of the field --` |
|     - | 2304 | `		 * but only when an enclosure is what it leads to. */` |
|   253 | 2305 | `		if( i < nLimit ){` |
|   233 | 2306 | `			int t = i;` |
|   480 | 2307 | `			while( t < nLimit && (unsigned char)zInput[t] != delim` |
|   367 | 2308 | `			 && SyisSpace((unsigned char)zInput[t]) ){` |
|    19 | 2309 | `				t++;` |
|     1 | 2310 | `			}` |
|   233 | 2311 | `			if( t < nLimit && (unsigned char)zInput[t] == encl ){` |
|    83 | 2312 | `				i = t;` |
|    41 | 2313 | `			}` |
|   116 | 2314 | `		}` |
|   253 | 2315 | `		if( bFirst && i >= nLimit ){` |
|     - | 2316 | `			/* A record with no text at all is ONE null field, not an empty one. */` |
|    13 | 2317 | `			PH7_MemObjInit(pVm,&sEntry);` |
|    13 | 2318 | `			ph7_array_add_elem(pArray,0,&sEntry);` |
|    13 | 2319 | `			PH7_MemObjRelease(&sEntry);` |
|    13 | 2320 | `			break;` |
|     - | 2321 | `		}` |
|   241 | 2322 | `		bFirst = 0;` |
|   241 | 2323 | `		bQuoted = (i < nLimit && (unsigned char)zInput[i] == encl);` |
|   241 | 2324 | `		if( bQuoted ){` |
|    83 | 2325 | `			int bClosed = 0;` |
|    83 | 2326 | `			i++;` |
|   281 | 2327 | `			while( i < nLimit ){` |
|   277 | 2328 | `				int c = (unsigned char)zInput[i];` |
|     - | 2329 | `				/* The ENCLOSURE is tested first, which only shows when the two` |
|     - | 2330 | `` 				 * are the same character: `str_getcsv('"aa"b', ',', '"', '"')` `` |
|     - | 2331 | `				 * closes on the second quote rather than escaping past it. (The` |
|     - | 2332 | `				 * WRITER's order is the other way round -- php's is too.) */` |
|   277 | 2333 | `				if( c == encl ){` |
|    91 | 2334 | `					if( i + 1 < nLimit && (unsigned char)zInput[i+1] == encl ){` |
|     - | 2335 | `						/* Doubled: one literal enclosure. */` |
|    13 | 2336 | `						SyBlobAppend(&sField,(const void *)&zInput[i],sizeof(char));` |
|    13 | 2337 | `						i += 2;` |
|    13 | 2338 | `						continue;` |
|     - | 2339 | `					}` |
|    79 | 2340 | `					i++;` |
|    79 | 2341 | `					bClosed = 1;` |
|    79 | 2342 | `					break;` |
|     - | 2343 | `				}` |
|   187 | 2344 | `				if( escape != PH7_CSV_NO_ESCAPE && c == escape ){` |
|     - | 2345 | `					/* php keeps the escape AND the byte it protects. */` |
|     5 | 2346 | `					SyBlobAppend(&sField,(const void *)&zInput[i],sizeof(char));` |
|     5 | 2347 | `					i++;` |
|     5 | 2348 | `					if( i < nLimit ){` |
|     5 | 2349 | `						SyBlobAppend(&sField,(const void *)&zInput[i],sizeof(char));` |
|     5 | 2350 | `						i++;` |
|     2 | 2351 | `					}` |
|     5 | 2352 | `					continue;` |
|     - | 2353 | `				}` |
|   183 | 2354 | `				SyBlobAppend(&sField,(const void *)&zInput[i],sizeof(char));` |
|   183 | 2355 | `				i++;` |
|     1 | 2356 | `			}` |
|    83 | 2357 | `			if( !bClosed ){` |
|     - | 2358 | `				/* The text ran out with the enclosure still open, so the line` |
|     - | 2359 | `				 * ending stripped off the top is INSIDE the value: php puts it` |
|     - | 2360 | `				 * back (which is also how a record that continues on the next` |
|     - | 2361 | `				 * line keeps its embedded newline). */` |
|     5 | 2362 | `				if( nByte > nLimit ){` |
|     7 | 2363 | `					SyBlobAppend(&sField,(const void *)&zInput[nLimit],` |
|     4 | 2364 | `						(sxu32)(nByte - nLimit));` |
|     2 | 2365 | `				}` |
|     5 | 2366 | `				if( pbOpen ){` |
|   ! 0 | 2367 | `					*pbOpen = 1;` |
|   ! 0 | 2368 | `				}` |
|     2 | 2369 | `			}` |
|     - | 2370 | `			/* Whatever trails the closing enclosure belongs to the field too. */` |
|   115 | 2371 | `			while( i < nLimit && (unsigned char)zInput[i] != delim ){` |
|    33 | 2372 | `				SyBlobAppend(&sField,(const void *)&zInput[i],sizeof(char));` |
|    33 | 2373 | `				i++;` |
|     1 | 2374 | `			}` |
|    42 | 2375 | `		}else{` |
|   159 | 2376 | `			int iStart = i;` |
|     - | 2377 | `			int nRaw;` |
|   337 | 2378 | `			while( i < nLimit && (unsigned char)zInput[i] != delim ){` |
|   179 | 2379 | `				i++;` |
|     1 | 2380 | `			}` |
|   159 | 2381 | `			nRaw = CsvStripEol(&zInput[iStart],i - iStart);` |
|   159 | 2382 | `			if( nRaw > 0 ){` |
|   139 | 2383 | `				SyBlobAppend(&sField,(const void *)&zInput[iStart],(sxu32)nRaw);` |
|    69 | 2384 | `			}` |
|     - | 2385 | `		}` |
|   241 | 2386 | `		PH7_MemObjInitFromString(pVm,&sEntry,0);` |
|   241 | 2387 | `		if( SyBlobLength(&sField) > 0 ){` |
|   322 | 2388 | `			ph7_value_string(&sEntry,(const char *)SyBlobData(&sField),` |
|   214 | 2389 | `				(int)SyBlobLength(&sField));` |
|   107 | 2390 | `		}` |
|   241 | 2391 | `		ph7_array_add_elem(pArray,0,&sEntry);` |
|   241 | 2392 | `		PH7_MemObjRelease(&sEntry);` |
|   241 | 2393 | `		if( i < nLimit && (unsigned char)zInput[i] == delim ){` |
|     - | 2394 | `			/* A trailing delimiter still opens one more (empty) field. */` |
|    99 | 2395 | `			i++;` |
|    99 | 2396 | `			continue;` |
|     - | 2397 | `		}` |
|   143 | 2398 | `		break;` |
|   ! 0 | 2399 | `	}` |
|   155 | 2400 | `	SyBlobRelease(&sField);` |
|   155 | 2401 | `	return SXRET_OK;` |
|     1 | 2402 | `}` |
|     - | 2403 | `/*` |
|     - | 2404 | ` * Validate a CSV $separator/$enclosure/$escape argument like php 8 and` |
|     - | 2405 | ` * extract its character. $separator/$enclosure must be exactly one` |
|     - | 2406 | ` * character; $escape may also be empty, which disables escape processing` |
|     - | 2407 | ` * (the caller gets PH7_CSV_NO_ESCAPE). The argument is coerced to string` |
|     - | 2408 | ` * first like php's ZPP, so an int 5 separates on "5"; null and array` |
|     - | 2409 | ` * arguments never reach here — the central type screen rejects them.` |
|     - | 2410 | ` * Returns PH7_OK on success; otherwise the ValueError has been thrown and` |
|     - | 2411 | ` * the caller must return the propagated status.` |
|     - | 2412 | ` */` |
|   790 | 2413 | `PH7_PRIVATE sxi32 PH7_CsvCharArg(ph7_context *pCtx,ph7_value *pArg,int iArg,` |
|     - | 2414 | `	const char *zName,int bAllowEmpty,int *pChar)` |
|     1 | 2415 | `{` |
|     - | 2416 | `	const char *zPtr;` |
|     - | 2417 | `	int n;` |
|   791 | 2418 | `	zPtr = ph7_value_to_string(pArg,&n);` |
|   791 | 2419 | `	if( n == 1 ){` |
|     - | 2420 | `		/* UNSIGNED: every comparison downstream is against a byte read out of a` |
|     - | 2421 | `		 * field, so a separator of "\xE9" stored as a negative char would match` |
|     - | 2422 | `		 * nothing at all and the field would go out unquoted. */` |
|   745 | 2423 | `		*pChar = (unsigned char)zPtr[0];` |
|   745 | 2424 | `		return PH7_OK;` |
|     - | 2425 | `	}` |
|    47 | 2426 | `	if( n < 1 && bAllowEmpty ){` |
|    15 | 2427 | `		*pChar = PH7_CSV_NO_ESCAPE;` |
|    15 | 2428 | `		return PH7_OK;` |
|     - | 2429 | `	}` |
|    49 | 2430 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 2431 | `		"%s(): Argument #%d ($%s) must be %sa single character",` |
|    16 | 2432 | `		ph7_function_name(pCtx),iArg,zName,bAllowEmpty ? "empty or " : ""` |
|     - | 2433 | `		);` |
|   396 | 2434 | `}` |
|     - | 2435 | `/*` |
|     - | 2436 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|     - | 2437 | ` *  Parse a CSV string into an array.` |
|     - | 2438 | ` * Parameters` |
|     - | 2439 | ` *  $input` |
|     - | 2440 | ` *   The string to parse.` |
|     - | 2441 | ` *  $delimiter` |
|     - | 2442 | ` *   Set the field delimiter (one character only).` |
|     - | 2443 | ` *  $enclosure` |
|     - | 2444 | ` *   Set the field enclosure character (one character only).` |
|     - | 2445 | ` *  $escape` |
|     - | 2446 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|     - | 2447 | ` * Return` |
|     - | 2448 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|     - | 2449 | ` */` |
|   120 | 2450 | `PH7_PRIVATE int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2451 | `{` |
|     - | 2452 | `	const char *zInput;` |
|     - | 2453 | `	ph7_value *pArray;` |
|   121 | 2454 | `	int delim  = ',';   /* Delimiter */` |
|   121 | 2455 | `	int encl   = '"' ;  /* Enclosure */` |
|   121 | 2456 | `	int escape = '\\';  /* Escape character */` |
|     - | 2457 | `	int nLen;` |
|   121 | 2458 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 2459 | `		/* Missing/Invalid arguments,return NULL */` |
|   ! 0 | 2460 | `		ph7_result_null(pCtx);` |
|   ! 0 | 2461 | `		return PH7_OK;` |
|     - | 2462 | `	}` |
|     - | 2463 | `	/* Extract the raw input */` |
|   121 | 2464 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|   121 | 2465 | `	if( nArg > 1 ){` |
|   119 | 2466 | `		sxi32 rc = PH7_CsvCharArg(pCtx,apArg[1],2,"separator",0,&delim);` |
|   119 | 2467 | `		if( rc != PH7_OK ){` |
|     5 | 2468 | `			return rc;` |
|     - | 2469 | `		}` |
|   115 | 2470 | `		if( nArg > 2 ){` |
|   115 | 2471 | `			rc = PH7_CsvCharArg(pCtx,apArg[2],3,"enclosure",0,&encl);` |
|   115 | 2472 | `			if( rc != PH7_OK ){` |
|     5 | 2473 | `				return rc;` |
|     - | 2474 | `			}` |
|   111 | 2475 | `			if( nArg > 3 ){` |
|   111 | 2476 | `				rc = PH7_CsvCharArg(pCtx,apArg[3],4,"escape",1,&escape);` |
|   111 | 2477 | `				if( rc != PH7_OK ){` |
|     3 | 2478 | `					return rc;` |
|     - | 2479 | `				}` |
|    54 | 2480 | `			}` |
|    54 | 2481 | `		}` |
|    54 | 2482 | `	}` |
|     - | 2483 | `	/* Create our array */` |
|   111 | 2484 | `	pArray = ph7_context_new_array(pCtx);` |
|   111 | 2485 | `	if( pArray == 0 ){` |
|     - | 2486 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|   ! 0 | 2487 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2488 | `	}` |
|     - | 2489 | `	/* Parse the raw input */` |
|   111 | 2490 | `	PH7_ProcessCsv(pArray,zInput,nLen,delim,encl,escape,0);` |
|     - | 2491 | `	/* Return the freshly created array */` |
|   111 | 2492 | `	ph7_result_value(pCtx,pArray);` |
|   111 | 2493 | `	return PH7_OK;` |
|    61 | 2494 | `}` |
|     - | 2495 | `/*` |
|     - | 2496 | `` * Is the collected tag (`<a href=...>`, `</a>`) one of the allowed ones?`` |
|     - | 2497 | ` *` |
|     - | 2498 | ` * php normalizes what it collected — lowercased, leading/trailing whitespace` |
|     - | 2499 | `` * dropped, everything after the tag NAME dropped, a closing `</x>` read as`` |
|     - | 2500 | `` * `<x>` — and then looks for that `<name>` as a SUBSTRING of the allow string,`` |
|     - | 2501 | `` * which is why the allow list is written as `"<a><b>"` and why a tag name that`` |
|     - | 2502 | ` * is a prefix of an allowed one is refused.` |
|     - | 2503 | ` */` |
|    66 | 2504 | `static int FvTagAllowed(const char *zTag,int nTag,const char *zAllow,int nAllow)` |
|     2 | 2505 | `{` |
|     - | 2506 | `	char zNorm[128];` |
|    68 | 2507 | `	int n = 0, i = 0, state = 0, done = 0;` |
|    68 | 2508 | `	if( nTag<1 \|\| nAllow<1 ){ return 0; }` |
|   338 | 2509 | `	while( !done && i<=nTag && n<(int)sizeof(zNorm)-2 ){` |
|     - | 2510 | `		/* php walks past the collected span into its NUL terminator */` |
|   272 | 2511 | `		int c = (i<nTag) ? SyToLower((unsigned char)zTag[i]) : 0;` |
|   272 | 2512 | `		switch( c ){` |
|    33 | 2513 | `		case '<':` |
|    68 | 2514 | `			zNorm[n++] = (char)c;` |
|    68 | 2515 | `			break;` |
|    32 | 2516 | `		case '>':` |
|    66 | 2517 | `			done = 1;` |
|    66 | 2518 | `			break;` |
|    70 | 2519 | `		default:` |
|   142 | 2520 | `			if( c && !SyisSpace((unsigned char)c) ){` |
|   140 | 2521 | `				if( state==0 ){ state = 1; }` |
|   140 | 2522 | `				if( c!='/' \|\| (i>0 && zTag[i-1]!='<' && !(i+1<nTag && zTag[i+1]=='>')) ){` |
|   106 | 2523 | `					zNorm[n++] = (char)c;` |
|    52 | 2524 | `				}` |
|    71 | 2525 | `			}else{` |
|     3 | 2526 | `				if( state==1 ){ done = 1; }` |
|     3 | 2527 | `				if( c==0 ){ done = 1; }` |
|     - | 2528 | `			}` |
|   140 | 2529 | `			break;` |
|     - | 2530 | `		}` |
|   272 | 2531 | `		i++;` |
|     2 | 2532 | `	}` |
|    68 | 2533 | `	zNorm[n++] = '>';` |
|    68 | 2534 | `	zNorm[n] = 0;` |
|     - | 2535 | `	/* the allow list is matched case-insensitively as a substring */` |
|    96 | 2536 | `	for( i=0; i+n<=nAllow; i++ ){` |
|     - | 2537 | `		int j;` |
|   244 | 2538 | `		for( j=0; j<n; j++ ){` |
|   198 | 2539 | `			if( SyToLower((unsigned char)zAllow[i+j]) != (unsigned char)zNorm[j] ){ break; }` |
|    86 | 2540 | `		}` |
|    76 | 2541 | `		if( j==n ){ return 1; }` |
|    16 | 2542 | `	}` |
|    22 | 2543 | `	return 0;` |
|    35 | 2544 | `}` |
|     - | 2545 | `/*` |
|     - | 2546 | ` * strip_tags(), a port of php's own state machine.` |
|     - | 2547 | ` *` |
|     - | 2548 | ` * State 0 is the output, state 1 an html tag, state 2 a php tag, state 3 an` |
|     - | 2549 | `` * `<!` construct and state 4 a comment. What the previous hand-rolled scan (find`` |
|     - | 2550 | ` * a '<', drop through to the next '>') got wrong is everything the machine` |
|     - | 2551 | ` * tracks beside the two brackets: a '<' followed by WHITESPACE is text and not a` |
|     - | 2552 | ` * tag opener; a quoted attribute may hold a '>' without closing the tag; a` |
|     - | 2553 | ` * nested '<' inside a tag raises a depth that the matching '>' lowers; a NUL is` |
|     - | 2554 | `` * dropped rather than ending the scan; `<?php ... ?>`, `<!-- ... -->` and`` |
|     - | 2555 | `` * `<!DOCTYPE ...>` each have their own exit rule; and an unterminated tag eats`` |
|     - | 2556 | ` * the rest of the input instead of reappearing as text.` |
|     - | 2557 | ` */` |
|    88 | 2558 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen,int bTagSpaces)` |
|     4 | 2559 | `{` |
|     - | 2560 | `	SyBlob sOut, sTag;` |
|    92 | 2561 | `	int i = 0, state = 0, depth = 0, in_q = 0, br = 0, is_xml = 0;` |
|    92 | 2562 | `	int lc = 0;` |
|    92 | 2563 | `	int bAllow = (zTaglist && nTaglen>0);` |
|    92 | 2564 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    92 | 2565 | `	SyBlobInit(&sTag,&pCtx->pVm->sAllocator);` |
|  1310 | 2566 | `	while( i<nByte ){` |
|  1222 | 2567 | `		unsigned char c = (unsigned char)zIn[i];` |
|     - | 2568 | `		/* php reads one byte past the current one; its buffer is NUL-terminated */` |
|  1222 | 2569 | `		unsigned char nx = (i+1<nByte) ? (unsigned char)zIn[i+1] : 0;` |
|  1222 | 2570 | `		switch( state ){` |
|   297 | 2571 | `		case 0:` |
|   598 | 2572 | `			if( c==0 ){` |
|     3 | 2573 | `				break;` |
|   596 | 2574 | `			}else if( c=='<' && !in_q ){` |
|   162 | 2575 | `				if( SyisSpace(nx) && !bTagSpaces ){` |
|     3 | 2576 | `					SyBlobAppend(&sOut,&zIn[i],1);` |
|     3 | 2577 | `					break;` |
|     - | 2578 | `				}` |
|   160 | 2579 | `				lc = '<';` |
|   160 | 2580 | `				state = 1;` |
|   160 | 2581 | `				if( bAllow ){ SyBlobAppend(&sTag,"<",1); }` |
|   160 | 2582 | `				i++;` |
|   160 | 2583 | `				continue;` |
|   438 | 2584 | `			}else if( c=='>' ){` |
|   ! 0 | 2585 | `				if( depth ){ depth--; break; }` |
|   ! 0 | 2586 | `				if( in_q ){ break; }` |
|   ! 0 | 2587 | `				SyBlobAppend(&sOut,&zIn[i],1);` |
|   ! 0 | 2588 | `			}else{` |
|   438 | 2589 | `				SyBlobAppend(&sOut,&zIn[i],1);` |
|     - | 2590 | `			}` |
|   438 | 2591 | `			break;` |
|   258 | 2592 | `		case 1:` |
|   520 | 2593 | `			if( c==0 ){` |
|     3 | 2594 | `				break;` |
|   518 | 2595 | `			}else if( c=='<' && !in_q ){` |
|     5 | 2596 | `				if( SyisSpace(nx) && !bTagSpaces ){` |
|   ! 0 | 2597 | `					if( bAllow ){ SyBlobAppend(&sTag,&zIn[i],1); }` |
|   ! 0 | 2598 | `					break;` |
|     - | 2599 | `				}` |
|     5 | 2600 | `				depth++;` |
|     5 | 2601 | `				break;` |
|   514 | 2602 | `			}else if( c=='>' ){` |
|   156 | 2603 | `				if( depth ){ depth--; break; }` |
|   152 | 2604 | `				if( in_q ){ break; }` |
|   148 | 2605 | `				lc = '>';` |
|   148 | 2606 | `				if( is_xml && i>=1 && zIn[i-1]=='-' ){ break; }` |
|   148 | 2607 | `				in_q = state = is_xml = 0;` |
|   148 | 2608 | `				if( bAllow ){` |
|    68 | 2609 | `					SyBlobAppend(&sTag,">",1);` |
|   101 | 2610 | `					if( FvTagAllowed((const char *)SyBlobData(&sTag),(int)SyBlobLength(&sTag),` |
|    33 | 2611 | `					                 zTaglist,nTaglen) ){` |
|    48 | 2612 | `						SyBlobAppend(&sOut,SyBlobData(&sTag),SyBlobLength(&sTag));` |
|    23 | 2613 | `					}` |
|    68 | 2614 | `					SyBlobReset(&sTag);` |
|    33 | 2615 | `				}` |
|   148 | 2616 | `				i++;` |
|   148 | 2617 | `				continue;` |
|   362 | 2618 | `			}else if( c=='"' \|\| c=='\'' ){` |
|    13 | 2619 | `				if( i!=0 && (!in_q \|\| (int)c==in_q) ){` |
|    13 | 2620 | `					in_q = in_q ? 0 : (int)c;` |
|     6 | 2621 | `				}` |
|    13 | 2622 | `				if( bAllow ){ SyBlobAppend(&sTag,&zIn[i],1); }` |
|   356 | 2623 | `			}else if( c=='!' && i>=1 && zIn[i-1]=='<' ){` |
|     5 | 2624 | `				state = 3;` |
|     5 | 2625 | `				lc = c;` |
|     5 | 2626 | `				i++;` |
|     5 | 2627 | `				continue;` |
|   346 | 2628 | `			}else if( c=='?' && i>=1 && zIn[i-1]=='<' ){` |
|     5 | 2629 | `				br = 0;` |
|     5 | 2630 | `				state = 2;` |
|     5 | 2631 | `				i++;` |
|     5 | 2632 | `				continue;` |
|   ! 0 | 2633 | `			}else{` |
|   342 | 2634 | `				if( bAllow ){ SyBlobAppend(&sTag,&zIn[i],1); }` |
|     - | 2635 | `			}` |
|   354 | 2636 | `			break;` |
|    37 | 2637 | `		case 2:` |
|    75 | 2638 | `			if( c=='(' ){` |
|   ! 0 | 2639 | `				if( lc!='"' && lc!='\'' ){ lc = '('; br++; }` |
|    75 | 2640 | `			}else if( c==')' ){` |
|   ! 0 | 2641 | `				if( lc!='"' && lc!='\'' ){ lc = ')'; br--; }` |
|    75 | 2642 | `			}else if( c=='>' ){` |
|     7 | 2643 | `				if( depth ){ depth--; break; }` |
|     7 | 2644 | `				if( in_q ){ break; }` |
|     5 | 2645 | `				if( !br && i>=1 && lc!='"' && zIn[i-1]=='?' ){` |
|     5 | 2646 | `					in_q = state = 0;` |
|     5 | 2647 | `					SyBlobReset(&sTag);` |
|     5 | 2648 | `					i++;` |
|     5 | 2649 | `					continue;` |
|   ! 0 | 2650 | `				}` |
|    69 | 2651 | `			}else if( c=='"' \|\| c=='\'' ){` |
|     9 | 2652 | `				if( i>=1 && zIn[i-1]!='\\' ){` |
|     9 | 2653 | `					if( lc==(int)c ){ lc = 0; }` |
|     5 | 2654 | `					else if( lc!='\\' ){ lc = (int)c; }` |
|     9 | 2655 | `					if( i!=0 && (!in_q \|\| (int)c==in_q) ){` |
|     9 | 2656 | `						in_q = in_q ? 0 : (int)c;` |
|     4 | 2657 | `					}` |
|     5 | 2658 | `				}` |
|    65 | 2659 | `			}else if( c=='l' \|\| c=='L' ){` |
|     - | 2660 | ``				/* `<?xml` is not php: back to the html state */`` |
|     2 | 2661 | `				if( state==2 && i>4` |
|     1 | 2662 | `				 && (zIn[i-1]=='m' \|\| zIn[i-1]=='M')` |
|   ! 0 | 2663 | `				 && (zIn[i-2]=='x' \|\| zIn[i-2]=='X')` |
|     1 | 2664 | `				 && zIn[i-3]=='?' && zIn[i-4]=='<' ){` |
|   ! 0 | 2665 | `					state = 1; is_xml = 1;` |
|   ! 0 | 2666 | `					i++;` |
|   ! 0 | 2667 | `					continue;` |
|     - | 2668 | `				}` |
|     1 | 2669 | `			}` |
|    69 | 2670 | `			break;` |
|     9 | 2671 | `		case 3:` |
|    19 | 2672 | `			if( c=='>' ){` |
|   ! 0 | 2673 | `				if( depth ){ depth--; break; }` |
|   ! 0 | 2674 | `				if( in_q ){ break; }` |
|   ! 0 | 2675 | `				in_q = state = 0;` |
|   ! 0 | 2676 | `				SyBlobReset(&sTag);` |
|   ! 0 | 2677 | `				i++;` |
|   ! 0 | 2678 | `				continue;` |
|    19 | 2679 | `			}else if( c=='"' \|\| c=='\'' ){` |
|   ! 0 | 2680 | `				if( i!=0 && zIn[i-1]!='\\' && (!in_q \|\| (int)c==in_q) ){` |
|   ! 0 | 2681 | `					in_q = in_q ? 0 : (int)c;` |
|   ! 0 | 2682 | `				}` |
|    19 | 2683 | `			}else if( c=='-' ){` |
|     5 | 2684 | `				if( i>=2 && zIn[i-1]=='-' && zIn[i-2]=='!' ){` |
|     3 | 2685 | `					state = 4;` |
|     3 | 2686 | `					i++;` |
|     3 | 2687 | `					continue;` |
|     1 | 2688 | `				}` |
|    16 | 2689 | `			}else if( c=='E' \|\| c=='e' ){` |
|     - | 2690 | `				/* the !DOCTYPE exception */` |
|     2 | 2691 | `				if( i>6` |
|     2 | 2692 | `				 && (zIn[i-1]=='p'\|\|zIn[i-1]=='P') && (zIn[i-2]=='y'\|\|zIn[i-2]=='Y')` |
|     2 | 2693 | `				 && (zIn[i-3]=='t'\|\|zIn[i-3]=='T') && (zIn[i-4]=='c'\|\|zIn[i-4]=='C')` |
|     3 | 2694 | `				 && (zIn[i-5]=='o'\|\|zIn[i-5]=='O') && (zIn[i-6]=='d'\|\|zIn[i-6]=='D') ){` |
|     3 | 2695 | `					state = 1;` |
|     3 | 2696 | `					i++;` |
|     3 | 2697 | `					continue;` |
|     - | 2698 | `				}` |
|   ! 0 | 2699 | `			}` |
|    15 | 2700 | `			break;` |
|     8 | 2701 | `		default: /* state 4: inside a comment */` |
|    17 | 2702 | `			if( c=='>' && !in_q && i>=2 && zIn[i-1]=='-' && zIn[i-2]=='-' ){` |
|     3 | 2703 | `				in_q = state = 0;` |
|     3 | 2704 | `				SyBlobReset(&sTag);` |
|     1 | 2705 | `			}` |
|    16 | 2706 | `			break;` |
|     - | 2707 | `		}` |
|   906 | 2708 | `		i++;` |
|     4 | 2709 | `	}` |
|    92 | 2710 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    92 | 2711 | `	SyBlobRelease(&sOut);` |
|    92 | 2712 | `	SyBlobRelease(&sTag);` |
|    92 | 2713 | `	return SXRET_OK;` |
|     4 | 2714 | `}` |
|     - | 2715 | `/*` |
|     - | 2716 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|     - | 2717 | ` *   Strip HTML and PHP tags from a string.` |
|     - | 2718 | ` * Parameters` |
|     - | 2719 | ` *  $str` |
|     - | 2720 | ` *  The input string.` |
|     - | 2721 | ` * $allowable_tags` |
|     - | 2722 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|     - | 2723 | ` * Return` |
|     - | 2724 | ` *  Returns the stripped string.` |
|     - | 2725 | ` */` |
|    14 | 2726 | `static int VmStripTagsListWalk(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|     1 | 2727 | `{` |
|    15 | 2728 | `	SyBlob *pOut = (SyBlob *)pUserData;` |
|     - | 2729 | `	ph7_value sTmp;` |
|     - | 2730 | `	const char *zTag; int nTag;` |
|     7 | 2731 | `	SXUNUSED(pKey);` |
|     - | 2732 | `	/* through a COPY: the array is the caller's */` |
|    15 | 2733 | `	PH7_MemObjInit(pData->pVm,&sTmp);` |
|    15 | 2734 | `	zTag = ph7_value_to_string(PH7_ValuePeek(pData,&sTmp),&nTag);` |
|    15 | 2735 | `	SyBlobAppend(pOut,"<",1);` |
|    15 | 2736 | `	if( nTag>0 ){ SyBlobAppend(pOut,zTag,(sxu32)nTag); }` |
|    15 | 2737 | `	SyBlobAppend(pOut,">",1);` |
|    15 | 2738 | `	PH7_MemObjRelease(&sTmp);` |
|    15 | 2739 | `	return PH7_OK;` |
|     1 | 2740 | `}` |
|    62 | 2741 | `PH7_PRIVATE int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 2742 | `{` |
|    65 | 2743 | `	const char *zTaglist = 0;` |
|     - | 2744 | `	const char *zString;` |
|     - | 2745 | `	SyBlob sTags;` |
|    65 | 2746 | `	int nTaglen = 0;` |
|     - | 2747 | `	int nLen;` |
|    65 | 2748 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 2749 | `		/* Missing/Invalid arguments,return the empty string */` |
|   ! 0 | 2750 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 2751 | `		return PH7_OK;` |
|     - | 2752 | `	}` |
|     - | 2753 | `	/* Point to the raw string */` |
|    65 | 2754 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    65 | 2755 | `	SyBlobInit(&sTags,&pCtx->pVm->sAllocator);` |
|    65 | 2756 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|     - | 2757 | `		/* Allowed tag */` |
|    14 | 2758 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|    59 | 2759 | `	}else if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|     - | 2760 | `		/* php 7.4's ARRAY spelling of the same list: each entry is a bare tag` |
|     - | 2761 | ``		 * NAME, and php builds the `<a><b>` string out of them. */`` |
|    15 | 2762 | `		ph7_array_walk(apArg[1],VmStripTagsListWalk,&sTags);` |
|    15 | 2763 | `		zTaglist = (const char *)SyBlobData(&sTags);` |
|    15 | 2764 | `		nTaglen = (int)SyBlobLength(&sTags);` |
|     7 | 2765 | `	}` |
|     - | 2766 | `	/* Process input */` |
|    65 | 2767 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen,0);` |
|    65 | 2768 | `	SyBlobRelease(&sTags);` |
|    65 | 2769 | `	return PH7_OK;` |
|    34 | 2770 | `}` |
|     - | 2771 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|     - | 2772 | `#ifdef PH7_NEED_FMT_AND_INI` |
|     - | 2773 | `/*` |
|     - | 2774 | ` * Parse an INI string.` |
|     - | 2775 |  |
|     - | 2776 | ` * According to wikipedia` |
|     - | 2777 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|     - | 2778 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|     - | 2779 | ` *  Format` |
|     - | 2780 | `*    Properties` |
|     - | 2781 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|     - | 2782 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|     - | 2783 | `*     Example:` |
|     - | 2784 | `*      name=value` |
|     - | 2785 | `*    Sections` |
|     - | 2786 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|     - | 2787 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|     - | 2788 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|     - | 2789 | `*     or the end of the file. Sections may not be nested.` |
|     - | 2790 | `*     Example:` |
|     - | 2791 | `*      [section]` |
|     - | 2792 | `*   Comments` |
|     - | 2793 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|     - | 2794 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|     - | 2795 | `*/` |
|     - | 2796 | `/*` |
|     - | 2797 | ` * The ini scanner's ${NAME} expansion: php answers a known ini OPTION first and` |
|     - | 2798 | ` * the process environment second (zend_ini_get_var), the empty string when` |
|     - | 2799 | ` * neither knows the name — a defined CONSTANT deliberately does NOT answer` |
|     - | 2800 | ` * here (that is the bare-identifier rule below). The VFS environment reader` |
|     - | 2801 | ` * answers through the call context's RESULT slot (it was written for` |
|     - | 2802 | ` * getenv()), so the read borrows pCtx->pRet around the call and empties it` |
|     - | 2803 | ` * again; the parse's own result is not written until the very end.` |
|     - | 2804 | ` */` |
|     6 | 2805 | `static void VmIniExpandDollarVar(ph7_context *pCtx,const char *zName,sxu32 nName,SyBlob *pOut)` |
|     1 | 2806 | `{` |
|     - | 2807 | `	char zVar[128];` |
|     - | 2808 | `	SyBlob sVal;` |
|     7 | 2809 | `	if( nName < 1 \|\| nName >= sizeof(zVar) ){` |
|   ! 0 | 2810 | `		return; /* php answers "" for an unknown name; an unreasonable one is unknown */` |
|     - | 2811 | `	}` |
|     7 | 2812 | `	SyMemcpy(zName,zVar,nName);` |
|     7 | 2813 | `	zVar[nName] = 0;` |
|     7 | 2814 | `	SyBlobInit(&sVal,&pCtx->pVm->sAllocator);` |
|     7 | 2815 | `	PH7_VmIniGetStr(pCtx->pVm,zVar,&sVal);` |
|     7 | 2816 | `	if( SyBlobLength(&sVal) > 0 ){` |
|   ! 0 | 2817 | `		SyBlobAppend(pOut,SyBlobData(&sVal),SyBlobLength(&sVal));` |
|   ! 0 | 2818 | `		SyBlobRelease(&sVal);` |
|   ! 0 | 2819 | `		return;` |
|     - | 2820 | `	}` |
|     7 | 2821 | `	SyBlobRelease(&sVal);` |
|     - | 2822 | `	{` |
|     7 | 2823 | `		const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     7 | 2824 | `		ph7_value *pRet = pCtx->pRet;` |
|     7 | 2825 | `		sxu32 nBefore = SyBlobLength(&pRet->sBlob);` |
|     7 | 2826 | `		if( pVfs && pVfs->xGetenv ){` |
|     7 | 2827 | `			if( pVfs->xGetenv(zVar,pCtx) == PH7_OK && SyBlobLength(&pRet->sBlob) > nBefore ){` |
|     7 | 2828 | `				SyBlobAppend(pOut,(const char *)SyBlobData(&pRet->sBlob) + nBefore,` |
|     4 | 2829 | `					SyBlobLength(&pRet->sBlob) - nBefore);` |
|     2 | 2830 | `			}` |
|     7 | 2831 | `			ph7_value_reset_string_cursor(pRet);` |
|     3 | 2832 | `		}` |
|     - | 2833 | `	}` |
|     4 | 2834 | `}` |
|     - | 2835 | `/*` |
|     - | 2836 | ` * Interpret one UNQUOTED ini value the way php's INI_SCANNER_NORMAL and` |
|     - | 2837 | ` * INI_SCANNER_TYPED do (a QUOTED value is always its literal bytes, and RAW` |
|     - | 2838 | ` * never reaches here):` |
|     - | 2839 | ` *` |
|     - | 2840 | ` *  - A whole-value word, case-insensitive: true/on/yes and false/off/no/none` |
|     - | 2841 | ` *    and null. NORMAL renders them "1" / "" / ""; TYPED renders true / false /` |
|     - | 2842 | ` *    NULL.` |
|     - | 2843 | ` *  - TYPED only: -?[0-9]+ is an int — a value int64 cannot hold falls back to` |
|     - | 2844 | ` *    the SOURCE text as a string — and [0-9]*\.[0-9]* with at least one digit` |
|     - | 2845 | ` *    is a float. php's typed grammar attaches '-' only to the INTEGER shape` |
|     - | 2846 | ` *    ("-1.5" stays a string); '+', hex, binary and exponents were never in it.` |
|     - | 2847 | ` *  - Everything else expands: ${NAME} answers an ini option or the` |
|     - | 2848 | ` *    environment, and a bare identifier token that names a DEFINED constant is` |
|     - | 2849 | ` *    replaced by that constant's value ("MYC and more" -> "someval and more").` |
|     - | 2850 | ` *` |
|     - | 2851 | ` * pValue arrives as an empty string.` |
|     - | 2852 | ` */` |
|   128 | 2853 | `static void VmIniInterpretValue(ph7_context *pCtx,const SyString *pRaw,int iMode,ph7_value *pValue)` |
|     1 | 2854 | `{` |
|   129 | 2855 | `	const char *zIn = pRaw->zString;` |
|   129 | 2856 | `	const char *zEnd = &zIn[pRaw->nByte];` |
|   129 | 2857 | `	sxu32 n = pRaw->nByte;` |
|     - | 2858 | `	SyBlob sOut;` |
|   129 | 2859 | `	if( n == 0 ){` |
|   ! 0 | 2860 | `		return; /* the empty string, both modes */` |
|     - | 2861 | `	}` |
|   128 | 2862 | `	if( (n == 4 && SyStrnicmp(zIn,"true",4) == 0)` |
|   125 | 2863 | `	 \|\| (n == 2 && SyStrnicmp(zIn,"on",2) == 0)` |
|   118 | 2864 | `	 \|\| (n == 3 && SyStrnicmp(zIn,"yes",3) == 0) ){` |
|    15 | 2865 | `		if( iMode == PH7_INI_SCANNER_TYPED ){` |
|    11 | 2866 | `			ph7_value_bool(pValue,1);` |
|     6 | 2867 | `		}else{` |
|     5 | 2868 | `			ph7_value_string(pValue,"1",1);` |
|     - | 2869 | `		}` |
|    15 | 2870 | `		return;` |
|     - | 2871 | `	}` |
|   114 | 2872 | `	if( (n == 5 && SyStrnicmp(zIn,"false",5) == 0)` |
|   113 | 2873 | `	 \|\| (n == 3 && SyStrnicmp(zIn,"off",3) == 0)` |
|   111 | 2874 | `	 \|\| (n == 2 && SyStrnicmp(zIn,"no",2) == 0)` |
|   111 | 2875 | `	 \|\| (n == 4 && SyStrnicmp(zIn,"none",4) == 0) ){` |
|    13 | 2876 | `		if( iMode == PH7_INI_SCANNER_TYPED ){` |
|     9 | 2877 | `			ph7_value_bool(pValue,0);` |
|     4 | 2878 | `		}` |
|     - | 2879 | `		/* NORMAL: the empty string pValue already holds */` |
|    13 | 2880 | `		return;` |
|     - | 2881 | `	}` |
|   109 | 2882 | `	if( n == 4 && SyStrnicmp(zIn,"null",4) == 0 ){` |
|     5 | 2883 | `		if( iMode == PH7_INI_SCANNER_TYPED ){` |
|     3 | 2884 | `			ph7_value_null(pValue);` |
|     1 | 2885 | `		}` |
|     5 | 2886 | `		return;` |
|     - | 2887 | `	}` |
|   105 | 2888 | `	if( iMode == PH7_INI_SCANNER_TYPED ){` |
|    33 | 2889 | `		sxu32 i = 0;` |
|    33 | 2890 | `		sxu32 nDig = 0,nDot = 0;` |
|    33 | 2891 | `		int bNeg = 0,bNum = 1;` |
|    33 | 2892 | `		if( zIn[0] == '-' ){` |
|     5 | 2893 | `			bNeg = 1;` |
|     5 | 2894 | `			i = 1;` |
|     2 | 2895 | `		}` |
|   153 | 2896 | `		for( ; i < n ; i++ ){` |
|   131 | 2897 | `			if( zIn[i] >= '0' && zIn[i] <= '9' ){` |
|   113 | 2898 | `				nDig++;` |
|    75 | 2899 | `			}else if( zIn[i] == '.' ){` |
|     9 | 2900 | `				nDot++;` |
|     5 | 2901 | `			}else{` |
|    11 | 2902 | `				bNum = 0;` |
|    11 | 2903 | `				break;` |
|     - | 2904 | `			}` |
|    61 | 2905 | `		}` |
|    33 | 2906 | `		if( bNum && nDig > 0 && nDot == 0 ){` |
|    15 | 2907 | `			sxi64 iVal = 0;` |
|    15 | 2908 | `			int iOverflow = 0;` |
|    15 | 2909 | `			SyStrToInt64Ex(zIn,n,(void *)&iVal,0,&iOverflow);` |
|    15 | 2910 | `			if( !iOverflow ){` |
|    13 | 2911 | `				ph7_value_int64(pValue,iVal);` |
|    13 | 2912 | `				return;` |
|     - | 2913 | `			}` |
|     3 | 2914 | `			ph7_value_string(pValue,zIn,(int)n);` |
|     3 | 2915 | `			return;` |
|     - | 2916 | `		}` |
|    19 | 2917 | `		if( bNum && nDig > 0 && nDot == 1 && !bNeg ){` |
|     7 | 2918 | `			double rVal = 0;` |
|     7 | 2919 | `			SyStrToReal(zIn,n,(void *)&rVal,0);` |
|     7 | 2920 | `			ph7_value_double(pValue,rVal);` |
|     7 | 2921 | `			return;` |
|     - | 2922 | `		}` |
|     6 | 2923 | `	}` |
|    85 | 2924 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   169 | 2925 | `	while( zIn < zEnd ){` |
|   119 | 2926 | `		if( zIn[0] == '$' && &zIn[1] < zEnd && zIn[1] == '{' ){` |
|     7 | 2927 | `			const char *p = &zIn[2];` |
|    97 | 2928 | `			while( p < zEnd && p[0] != '}' ){` |
|    91 | 2929 | `				p++;` |
|     1 | 2930 | `			}` |
|     7 | 2931 | `			if( p < zEnd ){` |
|     7 | 2932 | `				VmIniExpandDollarVar(pCtx,&zIn[2],(sxu32)(p - &zIn[2]),&sOut);` |
|     7 | 2933 | `				zIn = &p[1];` |
|     7 | 2934 | `				continue;` |
|     - | 2935 | `			}` |
|     - | 2936 | `			/* No closing brace: the bytes stand as written */` |
|   ! 0 | 2937 | `		}` |
|   113 | 2938 | `		if( ((unsigned char)zIn[0] < 0xc0 && SyisAlpha(zIn[0])) \|\| zIn[0] == '_' ){` |
|    79 | 2939 | `			const char *pTok = zIn;` |
|     - | 2940 | `			ph7_value sCons;` |
|   218 | 2941 | `			while( zIn < zEnd` |
|   281 | 2942 | `			 && (((unsigned char)zIn[0] < 0xc0 && SyisAlphaNum(zIn[0])) \|\| zIn[0] == '_') ){` |
|   237 | 2943 | `				zIn++;` |
|     1 | 2944 | `			}` |
|    45 | 2945 | `			PH7_MemObjInit(pCtx->pVm,&sCons);` |
|    45 | 2946 | `			if( PH7_VmQueryConstant(pCtx->pVm,pTok,(sxu32)(zIn - pTok),&sCons) ){` |
|     - | 2947 | `				int nCons;` |
|     5 | 2948 | `				const char *zCons = ph7_value_to_string(&sCons,&nCons);` |
|     5 | 2949 | `				SyBlobAppend(&sOut,zCons,(sxu32)nCons);` |
|     3 | 2950 | `			}else{` |
|    41 | 2951 | `				SyBlobAppend(&sOut,pTok,(sxu32)(zIn - pTok));` |
|     - | 2952 | `			}` |
|    45 | 2953 | `			PH7_MemObjRelease(&sCons);` |
|    45 | 2954 | `			continue;` |
|     - | 2955 | `		}` |
|    35 | 2956 | `		SyBlobAppend(&sOut,zIn,(sxu32)sizeof(char));` |
|    35 | 2957 | `		zIn++;` |
|     1 | 2958 | `	}` |
|    51 | 2959 | `	if( SyBlobLength(&sOut) > 0 ){` |
|    49 | 2960 | `		ph7_value_string(pValue,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    24 | 2961 | `	}` |
|    51 | 2962 | `	SyBlobRelease(&sOut);` |
|    51 | 2963 | `}` |
|    26 | 2964 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection,int iScannerMode)` |
|     1 | 2965 | `{` |
|     - | 2966 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|    27 | 2967 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|     - | 2968 | `	SyHashEntry *pEntry;` |
|     - | 2969 | `	SyString sEntry;` |
|     - | 2970 | `	SyHash sHash;` |
|     - | 2971 | `	int c;` |
|     - | 2972 | `	/* Create an empty array and worker variables */` |
|    27 | 2973 | `	pArray = ph7_context_new_array(pCtx);` |
|    27 | 2974 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|    27 | 2975 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    27 | 2976 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|     - | 2977 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|   ! 0 | 2978 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2979 | `	}` |
|    27 | 2980 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|    27 | 2981 | `	pCur = pArray;` |
|     - | 2982 | `	/* Start the parse process */` |
|    96 | 2983 | `	for(;;){` |
|     - | 2984 | `		/* Ignore leading white spaces */` |
|   355 | 2985 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|   159 | 2986 | `			zIn++;` |
|     1 | 2987 | `		}` |
|   197 | 2988 | `		if( zIn >= zEnd ){` |
|     - | 2989 | `			/* No more input to process */` |
|    27 | 2990 | `			break;` |
|     - | 2991 | `		}` |
|   171 | 2992 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|     - | 2993 | `			/* Comment til the end of line */` |
|     5 | 2994 | `			zIn++;` |
|    73 | 2995 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    69 | 2996 | `				zIn++;` |
|     1 | 2997 | `			}` |
|     5 | 2998 | `			continue;` |
|     - | 2999 | `		}` |
|     - | 3000 | `		/* Reset the string cursor of the working variable */` |
|   167 | 3001 | `		ph7_value_reset_string_cursor(pWorker);` |
|   167 | 3002 | `		if( zIn[0] == '[' ){` |
|     - | 3003 | `			/* Section: Extract the section name */` |
|    11 | 3004 | `			zIn++;` |
|    11 | 3005 | `			zCur = zIn;` |
|    77 | 3006 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|    67 | 3007 | `				zIn++;` |
|     1 | 3008 | `			}` |
|    11 | 3009 | `			if( zIn > zCur && bProcessSection ){` |
|     - | 3010 | `				/* Save the section name */` |
|     7 | 3011 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     7 | 3012 | `				SyStringFullTrim(&sEntry);` |
|     7 | 3013 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     7 | 3014 | `				if( sEntry.nByte > 0 ){` |
|     - | 3015 | `					/* Associate an array with the section */` |
|     7 | 3016 | `					pSection = ph7_context_new_array(pCtx);` |
|     7 | 3017 | `					if( pSection ){` |
|     7 | 3018 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|     7 | 3019 | `						pCur = pSection;` |
|     3 | 3020 | `					}` |
|     3 | 3021 | `				}` |
|     3 | 3022 | `			}` |
|    11 | 3023 | `			zIn++; /* Trailing square brackets ']' */` |
|     6 | 3024 | `		}else{` |
|     - | 3025 | `			ph7_value *pOldCur;` |
|     - | 3026 | `			int is_array;` |
|     - | 3027 | `			int iLen;` |
|     - | 3028 | `			/* Properties */` |
|   157 | 3029 | `			is_array = 0;` |
|   157 | 3030 | `			zCur = zIn;` |
|   157 | 3031 | `			iLen = 0; /* cc warning */` |
|   157 | 3032 | `			pOldCur = pCur;` |
|   681 | 3033 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|   525 | 3034 | `				if( zIn[0] == '[' && !is_array ){` |
|     - | 3035 | `					/* Array */` |
|   ! 0 | 3036 | `					iLen = (int)(zIn-zCur);` |
|   ! 0 | 3037 | `					is_array = 1;` |
|   ! 0 | 3038 | `					if( iLen > 0 ){` |
|   ! 0 | 3039 | `						ph7_value *pvArr = 0; /* cc warning */` |
|     - | 3040 | `						/* Query the hashtable */` |
|   ! 0 | 3041 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|   ! 0 | 3042 | `						SyStringFullTrim(&sEntry);` |
|   ! 0 | 3043 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|   ! 0 | 3044 | `						if( pEntry ){` |
|   ! 0 | 3045 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|   ! 0 | 3046 | `						}else{` |
|     - | 3047 | `							/* Create an empty array */` |
|   ! 0 | 3048 | `							pvArr = ph7_context_new_array(pCtx);` |
|   ! 0 | 3049 | `							if( pvArr ){` |
|     - | 3050 | `								/* Save the entry */` |
|   ! 0 | 3051 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|     - | 3052 | `								/* Insert the entry */` |
|   ! 0 | 3053 | `								ph7_value_reset_string_cursor(pWorker);` |
|   ! 0 | 3054 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|   ! 0 | 3055 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|   ! 0 | 3056 | `								ph7_value_reset_string_cursor(pWorker);` |
|   ! 0 | 3057 | `							}` |
|     - | 3058 | `						}` |
|   ! 0 | 3059 | `						if( pvArr ){` |
|   ! 0 | 3060 | `							pCur = pvArr;` |
|   ! 0 | 3061 | `						}` |
|   ! 0 | 3062 | `					}` |
|   ! 0 | 3063 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|   ! 0 | 3064 | `						zIn++;` |
|   ! 0 | 3065 | `					}` |
|   ! 0 | 3066 | `				}` |
|   525 | 3067 | `				zIn++;` |
|     1 | 3068 | `			}` |
|   157 | 3069 | `			if( !is_array ){` |
|   157 | 3070 | `				iLen = (int)(zIn-zCur);` |
|    78 | 3071 | `			}` |
|     - | 3072 | `			/* Trim the key */` |
|   157 | 3073 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|   307 | 3074 | `			SyStringFullTrim(&sEntry);` |
|   157 | 3075 | `			if( sEntry.nByte > 0 ){` |
|   157 | 3076 | `				if( !is_array ){` |
|     - | 3077 | `					/* Save the key name */` |
|   157 | 3078 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    78 | 3079 | `				}` |
|     - | 3080 | `				/* extract key value. pValue must come back to an EMPTY STRING` |
|     - | 3081 | `				 * whatever the last entry typed it as (INI_SCANNER_TYPED sets` |
|     - | 3082 | `				 * bool/int/float/null): ph7_value_string() re-types it, the` |
|     - | 3083 | `				 * cursor reset then empties it. */` |
|   157 | 3084 | `				ph7_value_string(pValue,"",0);` |
|   157 | 3085 | `				ph7_value_reset_string_cursor(pValue);` |
|   157 | 3086 | `				zIn++; /* '=' */` |
|     - | 3087 | `				/* Skip the spaces BEFORE the value but never the newline that` |
|     - | 3088 | ``				 * ENDS it: `key =` at end of line is php's empty-string entry,`` |
|     - | 3089 | `				 * and the old skip ran onto the next line and swallowed it` |
|     - | 3090 | ``				 * whole — `e1 =` followed by `c1 = 10K` answered`` |
|     - | 3091 | `				 * ["e1" => "c1 = 10K"] with c1 GONE. */` |
|   297 | 3092 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && zIn[0] != '\n' && SyisSpace(zIn[0]) ){` |
|   141 | 3093 | `					zIn++;` |
|     1 | 3094 | `				}` |
|   157 | 3095 | `				if( zIn < zEnd && zIn[0] != '\n' ){` |
|     - | 3096 | `					int bQuoted;` |
|   145 | 3097 | `					zCur = zIn;` |
|   145 | 3098 | `					c = zIn[0];` |
|   145 | 3099 | `					bQuoted = (c == '"' \|\| c == '\'');` |
|   145 | 3100 | `					if( bQuoted ){` |
|    13 | 3101 | `						zIn++;` |
|     - | 3102 | `						/* Delimit the value */` |
|   101 | 3103 | `						while( zIn < zEnd ){` |
|   101 | 3104 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    13 | 3105 | `								break;` |
|     - | 3106 | `							}` |
|    89 | 3107 | `							zIn++;` |
|     1 | 3108 | `						}` |
|    13 | 3109 | `						if( zIn < zEnd ){` |
|    13 | 3110 | `							zIn++;` |
|     6 | 3111 | `						}` |
|     7 | 3112 | `					}else{` |
|  1003 | 3113 | `						while( zIn < zEnd ){` |
|   997 | 3114 | `							if( zIn[0] == '\n' ){` |
|   123 | 3115 | `								if( zIn[-1] != '\\' ){` |
|   123 | 3116 | `									break;` |
|   ! 0 | 3117 | `								}` |
|   875 | 3118 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|     - | 3119 | `								/* Inline comments */` |
|     3 | 3120 | `								break;` |
|     - | 3121 | `							}` |
|   871 | 3122 | `							zIn++;` |
|     1 | 3123 | `						}` |
|     - | 3124 | `					}` |
|     - | 3125 | `					/* Trim the value */` |
|   145 | 3126 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|   149 | 3127 | `					SyStringFullTrim(&sEntry);` |
|   145 | 3128 | `					if( bQuoted ){` |
|    25 | 3129 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    25 | 3130 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|     6 | 3131 | `					}` |
|   145 | 3132 | `					if( bQuoted \|\| iScannerMode == PH7_INI_SCANNER_RAW ){` |
|     - | 3133 | `						/* A quoted value is its literal bytes in EVERY mode` |
|     - | 3134 | `						 * (php runs no expansion inside quotes), and RAW keeps` |
|     - | 3135 | `						 * even a bare word uninterpreted. */` |
|    45 | 3136 | `						if( sEntry.nByte > 0 ){` |
|    45 | 3137 | `							ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|    22 | 3138 | `						}` |
|    23 | 3139 | `					}else{` |
|   101 | 3140 | `						VmIniInterpretValue(pCtx,&sEntry,iScannerMode,pValue);` |
|     - | 3141 | `					}` |
|    72 | 3142 | `				}` |
|     - | 3143 | `				/* Insert the key and it's value (an empty value included) */` |
|   157 | 3144 | `				ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|    79 | 3145 | `			}else{` |
|   ! 0 | 3146 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|   ! 0 | 3147 | `					zIn++;` |
|   ! 0 | 3148 | `				}` |
|     - | 3149 | `			}` |
|   157 | 3150 | `			pCur = pOldCur;` |
|     - | 3151 | `		}` |
|     1 | 3152 | `	}` |
|    27 | 3153 | `	SyHashRelease(&sHash);` |
|     - | 3154 | `	/* Return the parse of the INI string */` |
|    27 | 3155 | `	ph7_result_value(pCtx,pArray);` |
|    27 | 3156 | `	return SXRET_OK;` |
|    14 | 3157 | `}` |
|     - | 3158 | `/*` |
|     - | 3159 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|     - | 3160 | ` *  Parse a configuration string.` |
|     - | 3161 | ` * Parameters` |
|     - | 3162 | ` *  $ini` |
|     - | 3163 | ` *   The contents of the ini file being parsed.` |
|     - | 3164 | ` *  $process_sections` |
|     - | 3165 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|     - | 3166 | ` *   and settings included. The default for process_sections is FALSE.` |
|     - | 3167 | ` *  $scanner_mode` |
|     - | 3168 | ` *   INI_SCANNER_NORMAL (default: values interpreted — booleans, constants,` |
|     - | 3169 | ` *   ${var}), INI_SCANNER_RAW (values kept verbatim) or INI_SCANNER_TYPED` |
|     - | 3170 | ` *   (booleans, null and numbers come back as their own types). Any other` |
|     - | 3171 | ` *   value is php's "Invalid scanner mode" warning and FALSE.` |
|     - | 3172 | ` * Return` |
|     - | 3173 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|     - | 3174 | ` */` |
|    22 | 3175 | `PH7_PRIVATE int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3176 | `{` |
|     - | 3177 | `	const char *zIni;` |
|     - | 3178 | `	int nByte;` |
|    23 | 3179 | `	int iMode = PH7_INI_SCANNER_NORMAL;` |
|    23 | 3180 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 3181 | `		/* Missing/Invalid arguments,return FALSE*/` |
|   ! 0 | 3182 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3183 | `		return PH7_OK;` |
|     - | 3184 | `	}` |
|    23 | 3185 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|    11 | 3186 | `		iMode = ph7_value_to_int(apArg[2]);` |
|    10 | 3187 | `		if( iMode != PH7_INI_SCANNER_NORMAL && iMode != PH7_INI_SCANNER_RAW` |
|     8 | 3188 | `		 && iMode != PH7_INI_SCANNER_TYPED ){` |
|     - | 3189 | ``			/* php's bare message: no `func(): ` qualifier on this one */`` |
|     3 | 3190 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"Invalid scanner mode");` |
|     3 | 3191 | `			ph7_result_bool(pCtx,0);` |
|     3 | 3192 | `			return PH7_OK;` |
|     - | 3193 | `		}` |
|     4 | 3194 | `	}` |
|     - | 3195 | `	/* Extract the raw INI buffer */` |
|    21 | 3196 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|     - | 3197 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|    21 | 3198 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0,iMode);` |
|    12 | 3199 | `}` |
|     - | 3200 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|     - | 3201 | `#ifdef PH7_NEED_BUILTIN_REG` |
|     - | 3202 |  |
|     - | 3203 | `/*` |
|     - | 3204 | ` * Ctype Functions.` |
|     - | 3205 | ` * Status:` |
|     - | 3206 | ` *    Stable.` |
|     - | 3207 | ` */` |
|     - | 3208 | `/*` |
|     - | 3209 | ` * bool ctype_alnum(string $text)` |
|     - | 3210 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|     - | 3211 | ` * Parameters` |
|     - | 3212 | ` *  $text` |
|     - | 3213 | ` *   The tested string.` |
|     - | 3214 | ` * Return` |
|     - | 3215 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|     - | 3216 | ` */` |
|    72 | 3217 | `PH7_PRIVATE int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3218 | `{` |
|     - | 3219 | `	const unsigned char *zIn,*zEnd;` |
|     - | 3220 | `	int nLen;` |
|    73 | 3221 | `	if( nArg < 1 ){` |
|     - | 3222 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 3223 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3224 | `		return PH7_OK;` |
|     - | 3225 | `	}` |
|     - | 3226 | `	/* Extract the target string */` |
|    73 | 3227 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    73 | 3228 | `	zEnd = &zIn[nLen];` |
|    73 | 3229 | `	if( nLen < 1 ){` |
|     - | 3230 | `		/* Empty string,return FALSE */` |
|     3 | 3231 | `		ph7_result_bool(pCtx,0);` |
|     3 | 3232 | `		return PH7_OK;` |
|     - | 3233 | `	}` |
|     - | 3234 | `	/* Perform the requested operation */` |
|   110 | 3235 | `	for(;;){` |
|   221 | 3236 | `		if( zIn >= zEnd ){` |
|     - | 3237 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    65 | 3238 | `			ph7_result_bool(pCtx,1);` |
|    65 | 3239 | `			return PH7_OK;` |
|     - | 3240 | `		}` |
|   157 | 3241 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|     7 | 3242 | `			break;` |
|     - | 3243 | `		}` |
|     - | 3244 | `		/* Point to the next character */` |
|   151 | 3245 | `		zIn++;` |
|     1 | 3246 | `	}` |
|     - | 3247 | `	/* The test failed,return FALSE */` |
|     7 | 3248 | `	ph7_result_bool(pCtx,0);` |
|     7 | 3249 | `	return PH7_OK;` |
|    37 | 3250 | `}` |
|     - | 3251 | `/*` |
|     - | 3252 | ` * bool ctype_alpha(string $text)` |
|     - | 3253 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|     - | 3254 | ` * Parameters` |
|     - | 3255 | ` *  $text` |
|     - | 3256 | ` *   The tested string.` |
|     - | 3257 | ` * Return` |
|     - | 3258 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|     - | 3259 | ` */` |
|    16 | 3260 | `PH7_PRIVATE int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3261 | `{` |
|     - | 3262 | `	const unsigned char *zIn,*zEnd;` |
|     - | 3263 | `	int nLen;` |
|    17 | 3264 | `	if( nArg < 1 ){` |
|     - | 3265 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 3266 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3267 | `		return PH7_OK;` |
|     - | 3268 | `	}` |
|     - | 3269 | `	/* Extract the target string */` |
|    17 | 3270 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 3271 | `	zEnd = &zIn[nLen];` |
|    17 | 3272 | `	if( nLen < 1 ){` |
|     - | 3273 | `		/* Empty string,return FALSE */` |
|     3 | 3274 | `		ph7_result_bool(pCtx,0);` |
|     3 | 3275 | `		return PH7_OK;` |
|     - | 3276 | `	}` |
|     - | 3277 | `	/* Perform the requested operation */` |
|    42 | 3278 | `	for(;;){` |
|    85 | 3279 | `		if( zIn >= zEnd ){` |
|     - | 3280 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 3281 | `			ph7_result_bool(pCtx,1);` |
|     9 | 3282 | `			return PH7_OK;` |
|     - | 3283 | `		}` |
|    77 | 3284 | `		if( !SyisAlpha(zIn[0]) ){` |
|     7 | 3285 | `			break;` |
|     - | 3286 | `		}` |
|     - | 3287 | `		/* Point to the next character */` |
|    71 | 3288 | `		zIn++;` |
|     1 | 3289 | `	}` |
|     - | 3290 | `	/* The test failed,return FALSE */` |
|     7 | 3291 | `	ph7_result_bool(pCtx,0);` |
|     7 | 3292 | `	return PH7_OK;` |
|     9 | 3293 | `}` |
|     - | 3294 | `/*` |
|     - | 3295 | ` * bool ctype_cntrl(string $text)` |
|     - | 3296 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|     - | 3297 | ` * Parameters` |
|     - | 3298 | ` *  $text` |
|     - | 3299 | ` *   The tested string.` |
|     - | 3300 | ` * Return` |
|     - | 3301 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|     - | 3302 | ` */` |
|    16 | 3303 | `PH7_PRIVATE int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3304 | `{` |
|     - | 3305 | `	const unsigned char *zIn,*zEnd;` |
|     - | 3306 | `	int nLen;` |
|    17 | 3307 | `	if( nArg < 1 ){` |
|     - | 3308 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 3309 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3310 | `		return PH7_OK;` |
|     - | 3311 | `	}` |
|     - | 3312 | `	/* Extract the target string */` |
|    17 | 3313 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 3314 | `	zEnd = &zIn[nLen];` |
|    17 | 3315 | `	if( nLen < 1 ){` |
|     - | 3316 | `		/* Empty string,return FALSE */` |
|     3 | 3317 | `		ph7_result_bool(pCtx,0);` |
|     3 | 3318 | `		return PH7_OK;` |
|     - | 3319 | `	}` |
|     - | 3320 | `	/* Perform the requested operation */` |
|    14 | 3321 | `	for(;;){` |
|    29 | 3322 | `		if( zIn >= zEnd ){` |
|     - | 3323 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 3324 | `			ph7_result_bool(pCtx,1);` |
|     9 | 3325 | `			return PH7_OK;` |
|     - | 3326 | `		}` |
|    21 | 3327 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 3328 | `			/* UTF-8 stream  */` |
|   ! 0 | 3329 | `			break;` |
|     - | 3330 | `		}` |
|    21 | 3331 | `		if( !SyisCtrl(zIn[0]) ){` |
|     7 | 3332 | `			break;` |
|     - | 3333 | `		}` |
|     - | 3334 | `		/* Point to the next character */` |
|    15 | 3335 | `		zIn++;` |
|     1 | 3336 | `	}` |
|     - | 3337 | `	/* The test failed,return FALSE */` |
|     7 | 3338 | `	ph7_result_bool(pCtx,0);` |
|     7 | 3339 | `	return PH7_OK;` |
|     9 | 3340 | `}` |
|     - | 3341 | `/*` |
|     - | 3342 | ` * bool ctype_digit(string $text)` |
|     - | 3343 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|     - | 3344 | ` * Parameters` |
|     - | 3345 | ` *  $text` |
|     - | 3346 | ` *   The tested string.` |
|     - | 3347 | ` * Return` |
|     - | 3348 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|     - | 3349 | ` */` |
|  1691 | 3350 | `PH7_PRIVATE int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 3351 | `{` |
|     - | 3352 | `	const unsigned char *zIn,*zEnd;` |
|     - | 3353 | `	int nLen;` |
|  1696 | 3354 | `	if( nArg < 1 ){` |
|     - | 3355 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 3356 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3357 | `		return PH7_OK;` |
|     - | 3358 | `	}` |
|     - | 3359 | `	/* Extract the target string */` |
|  1696 | 3360 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  1696 | 3361 | `	zEnd = &zIn[nLen];` |
|  1696 | 3362 | `	if( nLen < 1 ){` |
|     - | 3363 | `		/* Empty string,return FALSE */` |
|     3 | 3364 | `		ph7_result_bool(pCtx,0);` |
|     3 | 3365 | `		return PH7_OK;` |
|     - | 3366 | `	}` |
|     - | 3367 | `	/* Perform the requested operation */` |
|  1528 | 3368 | `	for(;;){` |
|  3059 | 3369 | `		if( zIn >= zEnd ){` |
|     - | 3370 | `			/* If we reach the end of the string,then the test succeeded. */` |
|  1304 | 3371 | `			ph7_result_bool(pCtx,1);` |
|  1304 | 3372 | `			return PH7_OK;` |
|     - | 3373 | `		}` |
|  1760 | 3374 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 3375 | `			/* UTF-8 stream  */` |
|   ! 0 | 3376 | `			break;` |
|     - | 3377 | `		}` |
|  1760 | 3378 | `		if( !SyisDigit(zIn[0]) ){` |
|   395 | 3379 | `			break;` |
|     - | 3380 | `		}` |
|     - | 3381 | `		/* Point to the next character */` |
|  1370 | 3382 | `		zIn++;` |
|     5 | 3383 | `	}` |
|     - | 3384 | `	/* The test failed,return FALSE */` |
|   395 | 3385 | `	ph7_result_bool(pCtx,0);` |
|   395 | 3386 | `	return PH7_OK;` |
|   851 | 3387 | `}` |
|     - | 3388 | `/*` |
|     - | 3389 | ` * bool ctype_xdigit(string $text)` |
|     - | 3390 | ` *  Check for character(s) representing a hexadecimal digit.` |
|     - | 3391 | ` * Parameters` |
|     - | 3392 | ` *  $text` |
|     - | 3393 | ` *   The tested string.` |
|     - | 3394 | ` * Return` |
|     - | 3395 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|     - | 3396 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|     - | 3397 | ` */` |
|   132 | 3398 | `PH7_PRIVATE int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 3399 | `{` |
|     - | 3400 | `	const unsigned char *zIn,*zEnd;` |
|     - | 3401 | `	int nLen;` |
|   134 | 3402 | `	if( nArg < 1 ){` |
|     - | 3403 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 3404 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3405 | `		return PH7_OK;` |
|     - | 3406 | `	}` |
|     - | 3407 | `	/* Extract the target string */` |
|   134 | 3408 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   134 | 3409 | `	zEnd = &zIn[nLen];` |
|   134 | 3410 | `	if( nLen < 1 ){` |
|     - | 3411 | `		/* Empty string,return FALSE */` |
|     3 | 3412 | `		ph7_result_bool(pCtx,0);` |
|     3 | 3413 | `		return PH7_OK;` |
|     - | 3414 | `	}` |
|     - | 3415 | `	/* Perform the requested operation */` |
|   217 | 3416 | `	for(;;){` |
|   436 | 3417 | `		if( zIn >= zEnd ){` |
|     - | 3418 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   126 | 3419 | `			ph7_result_bool(pCtx,1);` |
|   126 | 3420 | `			return PH7_OK;` |
|     - | 3421 | `		}` |
|   312 | 3422 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 3423 | `			/* UTF-8 stream  */` |
|   ! 0 | 3424 | `			break;` |
|     - | 3425 | `		}` |
|   312 | 3426 | `		if( !SyisHex(zIn[0]) ){` |
|     7 | 3427 | `			break;` |
|     - | 3428 | `		}` |
|     - | 3429 | `		/* Point to the next character */` |
|   306 | 3430 | `		zIn++;` |
|     2 | 3431 | `	}` |
|     - | 3432 | `	/* The test failed,return FALSE */` |
|     7 | 3433 | `	ph7_result_bool(pCtx,0);` |
|     7 | 3434 | `	return PH7_OK;` |
|    68 | 3435 | `}` |
|     - | 3436 | `/*` |
|     - | 3437 | ` * bool ctype_graph(string $text)` |
|     - | 3438 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|     - | 3439 | ` * Parameters` |
|     - | 3440 | ` *  $text` |
|     - | 3441 | ` *   The tested string.` |
|     - | 3442 | ` * Return` |
|     - | 3443 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|     - | 3444 | ` * (no white space), FALSE otherwise.` |
|     - | 3445 | ` */` |
|    16 | 3446 | `PH7_PRIVATE int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3447 | `{` |
|     - | 3448 | `	const unsigned char *zIn,*zEnd;` |
|     - | 3449 | `	int nLen;` |
|    17 | 3450 | `	if( nArg < 1 ){` |
|     - | 3451 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 3452 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3453 | `		return PH7_OK;` |
|     - | 3454 | `	}` |
|     - | 3455 | `	/* Extract the target string */` |
|    17 | 3456 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 3457 | `	zEnd = &zIn[nLen];` |
|    17 | 3458 | `	if( nLen < 1 ){` |
|     - | 3459 | `		/* Empty string,return FALSE */` |
|     3 | 3460 | `		ph7_result_bool(pCtx,0);` |
|     3 | 3461 | `		return PH7_OK;` |
|     - | 3462 | `	}` |
|     - | 3463 | `	/* Perform the requested operation */` |
|    57 | 3464 | `	for(;;){` |
|   115 | 3465 | `		if( zIn >= zEnd ){` |
|     - | 3466 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 3467 | `			ph7_result_bool(pCtx,1);` |
|     9 | 3468 | `			return PH7_OK;` |
|     - | 3469 | `		}` |
|   107 | 3470 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 3471 | `			/* UTF-8 stream  */` |
|   ! 0 | 3472 | `			break;` |
|     - | 3473 | `		}` |
|   107 | 3474 | `		if( !SyisGraph(zIn[0]) ){` |
|     7 | 3475 | `			break;` |
|     - | 3476 | `		}` |
|     - | 3477 | `		/* Point to the next character */` |
|   101 | 3478 | `		zIn++;` |
|     1 | 3479 | `	}` |
|     - | 3480 | `	/* The test failed,return FALSE */` |
|     7 | 3481 | `	ph7_result_bool(pCtx,0);` |
|     7 | 3482 | `	return PH7_OK;` |
|     9 | 3483 | `}` |
|     - | 3484 | `/*` |
|     - | 3485 | ` * bool ctype_print(string $text)` |
|     - | 3486 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|     - | 3487 | ` * Parameters` |
|     - | 3488 | ` *  $text` |
|     - | 3489 | ` *   The tested string.` |
|     - | 3490 | ` * Return` |
|     - | 3491 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|     - | 3492 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|     - | 3493 | ` *  or control function at all.` |
|     - | 3494 | ` */` |
|    16 | 3495 | `PH7_PRIVATE int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3496 | `{` |
|     - | 3497 | `	const unsigned char *zIn,*zEnd;` |
|     - | 3498 | `	int nLen;` |
|    17 | 3499 | `	if( nArg < 1 ){` |
|     - | 3500 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 3501 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3502 | `		return PH7_OK;` |
|     - | 3503 | `	}` |
|     - | 3504 | `	/* Extract the target string */` |
|    17 | 3505 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 3506 | `	zEnd = &zIn[nLen];` |
|    17 | 3507 | `	if( nLen < 1 ){` |
|     - | 3508 | `		/* Empty string,return FALSE */` |
|     3 | 3509 | `		ph7_result_bool(pCtx,0);` |
|     3 | 3510 | `		return PH7_OK;` |
|     - | 3511 | `	}` |
|     - | 3512 | `	/* Perform the requested operation */` |
|    63 | 3513 | `	for(;;){` |
|   127 | 3514 | `		if( zIn >= zEnd ){` |
|     - | 3515 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 3516 | `			ph7_result_bool(pCtx,1);` |
|     9 | 3517 | `			return PH7_OK;` |
|     - | 3518 | `		}` |
|   119 | 3519 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 3520 | `			/* UTF-8 stream  */` |
|   ! 0 | 3521 | `			break;` |
|     - | 3522 | `		}` |
|   119 | 3523 | `		if( !SyisPrint(zIn[0]) ){` |
|     7 | 3524 | `			break;` |
|     - | 3525 | `		}` |
|     - | 3526 | `		/* Point to the next character */` |
|   113 | 3527 | `		zIn++;` |
|     1 | 3528 | `	}` |
|     - | 3529 | `	/* The test failed,return FALSE */` |
|     7 | 3530 | `	ph7_result_bool(pCtx,0);` |
|     7 | 3531 | `	return PH7_OK;` |
|     9 | 3532 | `}` |
|     - | 3533 | `/*` |
|     - | 3534 | ` * bool ctype_punct(string $text)` |
|     - | 3535 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|     - | 3536 | ` * Parameters` |
|     - | 3537 | ` *  $text` |
|     - | 3538 | ` *   The tested string.` |
|     - | 3539 | ` * Return` |
|     - | 3540 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|     - | 3541 | ` *  digit or blank, FALSE otherwise.` |
|     - | 3542 | ` */` |
|    18 | 3543 | `PH7_PRIVATE int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3544 | `{` |
|     - | 3545 | `	const unsigned char *zIn,*zEnd;` |
|     - | 3546 | `	int nLen;` |
|    19 | 3547 | `	if( nArg < 1 ){` |
|     - | 3548 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 3549 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3550 | `		return PH7_OK;` |
|     - | 3551 | `	}` |
|     - | 3552 | `	/* Extract the target string */` |
|    19 | 3553 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    19 | 3554 | `	zEnd = &zIn[nLen];` |
|    19 | 3555 | `	if( nLen < 1 ){` |
|     - | 3556 | `		/* Empty string,return FALSE */` |
|     3 | 3557 | `		ph7_result_bool(pCtx,0);` |
|     3 | 3558 | `		return PH7_OK;` |
|     - | 3559 | `	}` |
|     - | 3560 | `	/* Perform the requested operation */` |
|    38 | 3561 | `	for(;;){` |
|    77 | 3562 | `		if( zIn >= zEnd ){` |
|     - | 3563 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 3564 | `			ph7_result_bool(pCtx,1);` |
|     9 | 3565 | `			return PH7_OK;` |
|     - | 3566 | `		}` |
|    69 | 3567 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 3568 | `			/* UTF-8 stream  */` |
|   ! 0 | 3569 | `			break;` |
|     - | 3570 | `		}` |
|    69 | 3571 | `		if( !SyisPunct(zIn[0]) ){` |
|     9 | 3572 | `			break;` |
|     - | 3573 | `		}` |
|     - | 3574 | `		/* Point to the next character */` |
|    61 | 3575 | `		zIn++;` |
|     1 | 3576 | `	}` |
|     - | 3577 | `	/* The test failed,return FALSE */` |
|     9 | 3578 | `	ph7_result_bool(pCtx,0);` |
|     9 | 3579 | `	return PH7_OK;` |
|    10 | 3580 | `}` |
|     - | 3581 | `/*` |
|     - | 3582 | ` * bool ctype_space(string $text)` |
|     - | 3583 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|     - | 3584 | ` * Parameters` |
|     - | 3585 | ` *  $text` |
|     - | 3586 | ` *   The tested string.` |
|     - | 3587 | ` * Return` |
|     - | 3588 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|     - | 3589 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|     - | 3590 | ` *  and form feed characters.` |
|     - | 3591 | ` */` |
| 42337 | 3592 | `PH7_PRIVATE int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 3593 | `{` |
|     - | 3594 | `	const unsigned char *zIn,*zEnd;` |
|     - | 3595 | `	int nLen;` |
| 42342 | 3596 | `	if( nArg < 1 ){` |
|     - | 3597 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 3598 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3599 | `		return PH7_OK;` |
|     - | 3600 | `	}` |
|     - | 3601 | `	/* Extract the target string */` |
| 42342 | 3602 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
| 42342 | 3603 | `	zEnd = &zIn[nLen];` |
| 42342 | 3604 | `	if( nLen < 1 ){` |
|     - | 3605 | `		/* Empty string,return FALSE */` |
|     3 | 3606 | `		ph7_result_bool(pCtx,0);` |
|     3 | 3607 | `		return PH7_OK;` |
|     - | 3608 | `	}` |
|     - | 3609 | `	/* Perform the requested operation */` |
| 21353 | 3610 | `	for(;;){` |
| 42374 | 3611 | `		if( zIn >= zEnd ){` |
|     - | 3612 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    11 | 3613 | `			ph7_result_bool(pCtx,1);` |
|    11 | 3614 | `			return PH7_OK;` |
|     - | 3615 | `		}` |
| 42364 | 3616 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 3617 | `			/* UTF-8 stream  */` |
|   ! 0 | 3618 | `			break;` |
|     - | 3619 | `		}` |
| 42364 | 3620 | `		if( !SyisSpace(zIn[0]) ){` |
| 42330 | 3621 | `			break;` |
|     - | 3622 | `		}` |
|     - | 3623 | `		/* Point to the next character */` |
|    35 | 3624 | `		zIn++;` |
|     1 | 3625 | `	}` |
|     - | 3626 | `	/* The test failed,return FALSE */` |
| 42330 | 3627 | `	ph7_result_bool(pCtx,0);` |
| 42330 | 3628 | `	return PH7_OK;` |
| 21342 | 3629 | `}` |
|     - | 3630 | `/*` |
|     - | 3631 | ` * bool ctype_lower(string $text)` |
|     - | 3632 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|     - | 3633 | ` * Parameters` |
|     - | 3634 | ` *  $text` |
|     - | 3635 | ` *   The tested string.` |
|     - | 3636 | ` * Return` |
|     - | 3637 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|     - | 3638 | ` */` |
|    16 | 3639 | `PH7_PRIVATE int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3640 | `{` |
|     - | 3641 | `	const unsigned char *zIn,*zEnd;` |
|     - | 3642 | `	int nLen;` |
|    17 | 3643 | `	if( nArg < 1 ){` |
|     - | 3644 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 3645 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3646 | `		return PH7_OK;` |
|     - | 3647 | `	}` |
|     - | 3648 | `	/* Extract the target string */` |
|    17 | 3649 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 3650 | `	zEnd = &zIn[nLen];` |
|    17 | 3651 | `	if( nLen < 1 ){` |
|     - | 3652 | `		/* Empty string,return FALSE */` |
|     3 | 3653 | `		ph7_result_bool(pCtx,0);` |
|     3 | 3654 | `		return PH7_OK;` |
|     - | 3655 | `	}` |
|     - | 3656 | `	/* Perform the requested operation */` |
|    27 | 3657 | `	for(;;){` |
|    55 | 3658 | `		if( zIn >= zEnd ){` |
|     - | 3659 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     5 | 3660 | `			ph7_result_bool(pCtx,1);` |
|     5 | 3661 | `			return PH7_OK;` |
|     - | 3662 | `		}` |
|    51 | 3663 | `		if( !SyisLower(zIn[0]) ){` |
|    11 | 3664 | `			break;` |
|     - | 3665 | `		}` |
|     - | 3666 | `		/* Point to the next character */` |
|    41 | 3667 | `		zIn++;` |
|     1 | 3668 | `	}` |
|     - | 3669 | `	/* The test failed,return FALSE */` |
|    11 | 3670 | `	ph7_result_bool(pCtx,0);` |
|    11 | 3671 | `	return PH7_OK;` |
|     9 | 3672 | `}` |
|     - | 3673 | `/*` |
|     - | 3674 | ` * bool ctype_upper(string $text)` |
|     - | 3675 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|     - | 3676 | ` * Parameters` |
|     - | 3677 | ` *  $text` |
|     - | 3678 | ` *   The tested string.` |
|     - | 3679 | ` * Return` |
|     - | 3680 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|     - | 3681 | ` */` |
|    16 | 3682 | `PH7_PRIVATE int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3683 | `{` |
|     - | 3684 | `	const unsigned char *zIn,*zEnd;` |
|     - | 3685 | `	int nLen;` |
|    17 | 3686 | `	if( nArg < 1 ){` |
|     - | 3687 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 3688 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3689 | `		return PH7_OK;` |
|     - | 3690 | `	}` |
|     - | 3691 | `	/* Extract the target string */` |
|    17 | 3692 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 3693 | `	zEnd = &zIn[nLen];` |
|    17 | 3694 | `	if( nLen < 1 ){` |
|     - | 3695 | `		/* Empty string,return FALSE */` |
|     3 | 3696 | `		ph7_result_bool(pCtx,0);` |
|     3 | 3697 | `		return PH7_OK;` |
|     - | 3698 | `	}` |
|     - | 3699 | `	/* Perform the requested operation */` |
|    28 | 3700 | `	for(;;){` |
|    57 | 3701 | `		if( zIn >= zEnd ){` |
|     - | 3702 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     5 | 3703 | `			ph7_result_bool(pCtx,1);` |
|     5 | 3704 | `			return PH7_OK;` |
|     - | 3705 | `		}` |
|    53 | 3706 | `		if( !SyisUpper(zIn[0]) ){` |
|    11 | 3707 | `			break;` |
|     - | 3708 | `		}` |
|     - | 3709 | `		/* Point to the next character */` |
|    43 | 3710 | `		zIn++;` |
|     1 | 3711 | `	}` |
|     - | 3712 | `	/* The test failed,return FALSE */` |
|    11 | 3713 | `	ph7_result_bool(pCtx,0);` |
|    11 | 3714 | `	return PH7_OK;` |
|     9 | 3715 | `}` |
|     - | 3716 | `/* Date/Time functions moved to builtin_date.c */` |
|     - | 3717 | `/*` |
|     - | 3718 | ` * Section:` |
|     - | 3719 | ` *    URL handling Functions.` |
|     - | 3720 | ` * Status:` |
|     - | 3721 | ` *    Stable.` |
|     - | 3722 | ` */` |
|     - | 3723 | `/*` |
|     - | 3724 | ` * Output consumer callback for the standard Symisc routines.` |
|     - | 3725 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|     - | 3726 | ` */` |
|  1830 | 3727 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|     3 | 3728 | `{` |
|     - | 3729 | `	/* Store in the call context result buffer */` |
|  1833 | 3730 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|  1833 | 3731 | `	return SXRET_OK;` |
|     3 | 3732 | `}` |
|     - | 3733 | `/*` |
|     - | 3734 | ` * string base64_encode(string $data)` |
|     - | 3735 | ` *  Encodes data with MIME base64` |
|     - | 3736 | ` * Parameter` |
|     - | 3737 | ` *  $data` |
|     - | 3738 | ` *    Data to encode` |
|     - | 3739 | ` * Return` |
|     - | 3740 | ` *  Encoded data or FALSE on failure.` |
|     - | 3741 | ` */` |
|    12 | 3742 | `PH7_PRIVATE int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 3743 | `{` |
|     - | 3744 | `	const char *zIn;` |
|     - | 3745 | `	int nLen;` |
|    14 | 3746 | `	if( nArg < 1 ){` |
|     - | 3747 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 3748 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3749 | `		return PH7_OK;` |
|     - | 3750 | `	}` |
|     - | 3751 | `	/* Extract the input string */` |
|    14 | 3752 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    14 | 3753 | `	if( nLen < 1 ){` |
|     - | 3754 | `		/* php encodes the empty string to the EMPTY STRING; base64_encode() cannot` |
|     - | 3755 | `		 * fail at all, so FALSE was never one of its answers. */` |
|     3 | 3756 | `		ph7_result_string(pCtx,"",0);` |
|     3 | 3757 | `		return PH7_OK;` |
|     - | 3758 | `	}` |
|     - | 3759 | `	/* Perform the BASE64 encoding */` |
|    12 | 3760 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|    12 | 3761 | `	return PH7_OK;` |
|     8 | 3762 | `}` |
|     - | 3763 | `/*` |
|     - | 3764 | ` * php's base64 reverse table: -1 is skippable whitespace (\t \n \r and space,` |
|     - | 3765 | ` * exactly php's set -- \v/\f are NOT skipped), -2 is an invalid byte, 0..63 the` |
|     - | 3766 | ` * decoded 6-bit value. The pad byte '=' is handled before the lookup, so its` |
|     - | 3767 | ` * table slot is never consulted.` |
|     - | 3768 | ` */` |
|     - | 3769 | `static const signed char aB64Rev[256] = {` |
|     - | 3770 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-1,-1,-2,-2,-1,-2,-2,` |
|     - | 3771 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 3772 | `	-1,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,62,-2,-2,-2,63,` |
|     - | 3773 | `	52,53,54,55,56,57,58,59,60,61,-2,-2,-2,-2,-2,-2,` |
|     - | 3774 | `	-2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,` |
|     - | 3775 | `	15,16,17,18,19,20,21,22,23,24,25,-2,-2,-2,-2,-2,` |
|     - | 3776 | `	-2,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,` |
|     - | 3777 | `	41,42,43,44,45,46,47,48,49,50,51,-2,-2,-2,-2,-2,` |
|     - | 3778 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 3779 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 3780 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 3781 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 3782 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 3783 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 3784 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 3785 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2` |
|     - | 3786 | `};` |
|     - | 3787 | `/*` |
|     - | 3788 | ` * string base64_decode(string $data, bool $strict = false)` |
|     - | 3789 | ` *  Decodes data encoded with MIME base64` |
|     - | 3790 | ` * Parameters` |
|     - | 3791 | ` *  $data` |
|     - | 3792 | ` *    Encoded data.` |
|     - | 3793 | ` *  $strict` |
|     - | 3794 | ` *    When true, return FALSE if the input contains a character outside the` |
|     - | 3795 | ` *    base64 alphabet (whitespace is still skipped) or the padding/length is` |
|     - | 3796 | ` *    malformed. When false, such bytes are silently skipped (best effort).` |
|     - | 3797 | ` * Return` |
|     - | 3798 | ` *  Returns the original data or FALSE on failure.` |
|     - | 3799 | ` * Implementation note: a faithful port of php's php_base64_decode_ex(). The old` |
|     - | 3800 | ` * code ignored $strict entirely and ran the shared SyBase64Decode(), which maps` |
|     - | 3801 | ` * every non-alphabet byte (whitespace included) to 0 rather than skipping it --` |
|     - | 3802 | ` * a silent wrong answer on padded/whitespace input in BOTH modes.` |
|     - | 3803 | ` */` |
|    40 | 3804 | `PH7_PRIVATE int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 3805 | `{` |
|     - | 3806 | `	const unsigned char *zIn;` |
|     - | 3807 | `	unsigned char *zOut;` |
|    42 | 3808 | `	int nLen,strict = 0;` |
|    42 | 3809 | `	int i = 0,j = 0,padding = 0,k;` |
|    42 | 3810 | `	if( nArg < 1 ){` |
|     - | 3811 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 3812 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3813 | `		return PH7_OK;` |
|     - | 3814 | `	}` |
|     - | 3815 | `	/* Extract the input string */` |
|    42 | 3816 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    42 | 3817 | `	if( nLen < 1 ){` |
|     - | 3818 | `		/* php decodes the empty string to the EMPTY STRING, not FALSE (FALSE is reserved` |
|     - | 3819 | `		 * for input that cannot be decoded at all). */` |
|     6 | 3820 | `		ph7_result_string(pCtx,"",0);` |
|     6 | 3821 | `		return PH7_OK;` |
|     - | 3822 | `	}` |
|    37 | 3823 | `	if( nArg > 1 ){` |
|    31 | 3824 | `		strict = ph7_value_to_bool(apArg[1]);` |
|    15 | 3825 | `	}` |
|     - | 3826 | `	/* Output is at most 3/4 of the input; nLen bytes is a safe upper bound. */` |
|    37 | 3827 | `	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nLen + 1);` |
|    37 | 3828 | `	if( zOut == 0 ){` |
|   ! 0 | 3829 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 3830 | `	}` |
|   187 | 3831 | `	for( k = 0 ; k < nLen ; ++k ){` |
|   155 | 3832 | `		int ch = zIn[k];` |
|     - | 3833 | `		int val;` |
|   155 | 3834 | `		if( ch == '=' ){` |
|     - | 3835 | `			/* Pad byte: count it, decode nothing. */` |
|    23 | 3836 | `			padding++;` |
|    23 | 3837 | `			continue;` |
|     - | 3838 | `		}` |
|   133 | 3839 | `		val = aB64Rev[ch];` |
|   133 | 3840 | `		if( !strict ){` |
|     - | 3841 | `			/* Lenient: skip whitespace AND any invalid byte. */` |
|    61 | 3842 | `			if( val < 0 ){` |
|    15 | 3843 | `				continue;` |
|     - | 3844 | `			}` |
|    24 | 3845 | `		}else{` |
|    73 | 3846 | `			if( val == -1 ){` |
|     - | 3847 | `				/* Skippable whitespace. */` |
|     7 | 3848 | `				continue;` |
|     - | 3849 | `			}` |
|    67 | 3850 | `			if( val == -2 ){` |
|     - | 3851 | `				/* A byte outside the base64 alphabet. */` |
|     5 | 3852 | `				goto fail;` |
|     - | 3853 | `			}` |
|    63 | 3854 | `			if( padding ){` |
|     - | 3855 | `				/* Data must not follow the padding. */` |
|   ! 0 | 3856 | `				goto fail;` |
|     - | 3857 | `			}` |
|     - | 3858 | `		}` |
|   109 | 3859 | `		switch( i & 3 ){` |
|    20 | 3860 | `			case 0:` |
|    41 | 3861 | `				zOut[j] = (unsigned char)(val << 2);` |
|    41 | 3862 | `				break;` |
|    18 | 3863 | `			case 1:` |
|    37 | 3864 | `				zOut[j++] \|= (unsigned char)(val >> 4);` |
|    37 | 3865 | `				zOut[j] = (unsigned char)((val & 0x0F) << 4);` |
|    37 | 3866 | `				break;` |
|    11 | 3867 | `			case 2:` |
|    23 | 3868 | `				zOut[j++] \|= (unsigned char)(val >> 2);` |
|    23 | 3869 | `				zOut[j] = (unsigned char)((val & 0x03) << 6);` |
|    23 | 3870 | `				break;` |
|     5 | 3871 | `			case 3:` |
|    11 | 3872 | `				zOut[j++] \|= (unsigned char)val;` |
|    10 | 3873 | `				break;` |
|     - | 3874 | `		}` |
|   109 | 3875 | `		i++;` |
|    55 | 3876 | `	}` |
|    33 | 3877 | `	if( strict ){` |
|     - | 3878 | `		/* A lone trailing 6-bit group (one leftover char) cannot form a byte. */` |
|    21 | 3879 | `		if( (i & 3) == 1 ){` |
|     3 | 3880 | `			goto fail;` |
|     - | 3881 | `		}` |
|     - | 3882 | `		/* Padding must be 1 or 2 bytes and complete the 4-char group. */` |
|    19 | 3883 | `		if( padding && (padding > 2 \|\| ((i + padding) & 3) != 0) ){` |
|     3 | 3884 | `			goto fail;` |
|     - | 3885 | `		}` |
|     8 | 3886 | `	}` |
|    29 | 3887 | `	ph7_result_string(pCtx,(const char *)zOut,j);` |
|    29 | 3888 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|    29 | 3889 | `	return PH7_OK;` |
|     4 | 3890 | `fail:` |
|     9 | 3891 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|     9 | 3892 | `	ph7_result_bool(pCtx,0);` |
|     9 | 3893 | `	return PH7_OK;` |
|    22 | 3894 | `}` |
|     - | 3895 | `/*` |
|     - | 3896 | ` * uuencode's six-bit alphabet: a value of 0 is written as the backtick php uses` |
|     - | 3897 | ` * instead of the historical space, every other value as ' ' + value. The three` |
|     - | 3898 | ` * PH7_UU_ENC_C* helpers pack the 6-bit groups exactly like php's macros: each` |
|     - | 3899 | ` * contribution is masked to its own bit window, so the result never depends on` |
|     - | 3900 | ` * whether the platform's char is signed.` |
|     - | 3901 | ` */` |
|     - | 3902 | ``#define PH7_UU_ENC(c)      ((char)((c) ? (((c) & 077) + ' ') : '`'))`` |
|     - | 3903 | `#define PH7_UU_ENC_C1(a)   PH7_UU_ENC((a) >> 2)` |
|     - | 3904 | `#define PH7_UU_ENC_C2(a,b) PH7_UU_ENC((((a) << 4) & 060) \| (((b) >> 4) & 017))` |
|     - | 3905 | `#define PH7_UU_ENC_C3(b,c) PH7_UU_ENC((((b) << 2) & 074) \| (((c) >> 6) & 003))` |
|     - | 3906 | `#define PH7_UU_ENC_C4(c)   PH7_UU_ENC((c) & 077)` |
|     - | 3907 | `#define PH7_UU_DEC(c)      ((((int)(c)) - ' ') & 077)` |
|     - | 3908 | `/*` |
|     - | 3909 | ` * string convert_uuencode(string $data)` |
|     - | 3910 | ` *  Uuencode a string.` |
|     - | 3911 | ` * Parameter` |
|     - | 3912 | ` *  $data` |
|     - | 3913 | ` *   Data to encode.` |
|     - | 3914 | ` * Return` |
|     - | 3915 | ` *  The uuencoded data: 45-byte lines, each prefixed with its encoded length and` |
|     - | 3916 | `` *  terminated by a newline, followed by php's "`\n" end marker. An empty input`` |
|     - | 3917 | ` *  answers just that marker.` |
|     - | 3918 | ` * Implementation note: a faithful port of php's php_uuencode(). This used to be` |
|     - | 3919 | ` * registered as an ALIAS of base64_encode() -- a wrong ALGORITHM, so every answer` |
|     - | 3920 | ` * was silently a base64 string (convert_uuencode("abc") gave "YWJj" where php` |
|     - | 3921 | `` * gives "#86)C\n`\n").`` |
|     - | 3922 | ` */` |
|    36 | 3923 | `PH7_PRIVATE int PH7_builtin_convert_uuencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 3924 | `{` |
|     - | 3925 | `	const unsigned char *zIn,*zEnd,*zStop;` |
|     - | 3926 | `	char zLine[64]; /* one full line is 1 length byte + 60 data bytes + '\n' */` |
|    38 | 3927 | `	int nLen,iLen = 45,n;` |
|    38 | 3928 | `	if( nArg < 1 ){` |
|     - | 3929 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 3930 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3931 | `		return PH7_OK;` |
|     - | 3932 | `	}` |
|     - | 3933 | `	/* Extract the input string */` |
|    38 | 3934 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    38 | 3935 | `	if( nLen < 0 ){` |
|   ! 0 | 3936 | `		nLen = 0;` |
|   ! 0 | 3937 | `	}` |
|    38 | 3938 | `	zEnd = &zIn[nLen];` |
|     - | 3939 | `	/* Emit whole groups while at least four bytes remain: the last line is closed by` |
|     - | 3940 | ``	 * the tail block below so a group of one or two bytes gets php's '`' filler. */`` |
|    70 | 3941 | `	while( &zIn[3] < zEnd ){` |
|    34 | 3942 | `		zStop = &zIn[iLen];` |
|    34 | 3943 | `		if( zStop > zEnd ){` |
|     - | 3944 | `			/* A short final line: its length byte counts every remaining byte, but only` |
|     - | 3945 | `			 * whole three-byte groups are encoded here -- the leftovers ride the tail` |
|     - | 3946 | `			 * block, which then adds no length byte of its own. */` |
|    10 | 3947 | `			iLen = (int)(zEnd - zIn);` |
|    10 | 3948 | `			zStop = &zIn[(iLen/3)*3];` |
|     4 | 3949 | `		}` |
|    34 | 3950 | `		n = 0;` |
|    34 | 3951 | `		zLine[n++] = PH7_UU_ENC(iLen);` |
|   476 | 3952 | `		while( zIn < zStop ){` |
|   444 | 3953 | `			zLine[n++] = PH7_UU_ENC_C1(zIn[0]);` |
|   444 | 3954 | `			zLine[n++] = PH7_UU_ENC_C2(zIn[0],zIn[1]);` |
|   444 | 3955 | `			zLine[n++] = PH7_UU_ENC_C3(zIn[1],zIn[2]);` |
|   444 | 3956 | `			zLine[n++] = PH7_UU_ENC_C4(zIn[2]);` |
|   444 | 3957 | `			zIn += 3;` |
|     2 | 3958 | `		}` |
|    34 | 3959 | `		if( iLen == 45 ){` |
|    25 | 3960 | `			zLine[n++] = '\n';` |
|    12 | 3961 | `		}` |
|    34 | 3962 | `		ph7_result_string(pCtx,zLine,n);` |
|     2 | 3963 | `	}` |
|    38 | 3964 | `	if( zIn < zEnd ){` |
|     - | 3965 | `		/* One to three trailing bytes. php reads the bytes past the end of the string` |
|     - | 3966 | `		 * (its buffers are NUL terminated); the missing ones are zero here. */` |
|    30 | 3967 | `		unsigned char c0 = zIn[0];` |
|    30 | 3968 | `		unsigned char c1 = (&zIn[1] < zEnd) ? zIn[1] : 0;` |
|    30 | 3969 | `		unsigned char c2 = (&zIn[2] < zEnd) ? zIn[2] : 0;` |
|    30 | 3970 | `		n = 0;` |
|    30 | 3971 | `		if( iLen == 45 ){` |
|     - | 3972 | `			/* No short line was opened above: this group is a line of its own. */` |
|    22 | 3973 | `			zLine[n++] = PH7_UU_ENC((int)(zEnd - zIn));` |
|    22 | 3974 | `			iLen = 0;` |
|    10 | 3975 | `		}` |
|    30 | 3976 | `		zLine[n++] = PH7_UU_ENC_C1(c0);` |
|    30 | 3977 | `		zLine[n++] = PH7_UU_ENC_C2(c0,c1);` |
|    30 | 3978 | ``		zLine[n++] = ((zEnd - zIn) > 1) ? PH7_UU_ENC_C3(c1,c2) : '`';`` |
|    30 | 3979 | ``		zLine[n++] = ((zEnd - zIn) > 2) ? PH7_UU_ENC_C4(c2)     : '`';`` |
|    30 | 3980 | `		ph7_result_string(pCtx,zLine,n);` |
|    14 | 3981 | `	}` |
|    38 | 3982 | `	if( iLen != 45 ){` |
|     - | 3983 | `		/* A short (or tail) line is still open; a run of whole 45-byte lines -- and the` |
|     - | 3984 | `		 * empty input, which opens no line at all -- is already newline-terminated. */` |
|    30 | 3985 | `		ph7_result_string(pCtx,"\n",1);` |
|    14 | 3986 | `	}` |
|     - | 3987 | `	/* php's end marker: a zero-length line. */` |
|    38 | 3988 | ``	ph7_result_string(pCtx,"`\n",2);`` |
|    38 | 3989 | `	return PH7_OK;` |
|    20 | 3990 | `}` |
|     - | 3991 | `/*` |
|     - | 3992 | ` * string\|false convert_uudecode(string $data)` |
|     - | 3993 | ` *  Decode a uuencoded string.` |
|     - | 3994 | ` * Parameter` |
|     - | 3995 | ` *  $data` |
|     - | 3996 | ` *   Uuencoded data.` |
|     - | 3997 | ` * Return` |
|     - | 3998 | ` *  The decoded data, or FALSE (with a warning) when $data is not a valid uuencoded` |
|     - | 3999 | ` *  string: an empty input, a line claiming more bytes than the whole input holds, or` |
|     - | 4000 | ` *  a line whose data is truncated. Trailing garbage after the first short line is` |
|     - | 4001 | ` *  ignored, exactly like php.` |
|     - | 4002 | ` * Implementation note: a faithful port of php's php_uudecode(); see the encoder above` |
|     - | 4003 | ` * for why this was not a decoder at all before.` |
|     - | 4004 | ` */` |
|    48 | 4005 | `PH7_PRIVATE int PH7_builtin_convert_uudecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 4006 | `{` |
|     - | 4007 | `	const unsigned char *zIn,*zEnd,*zStop;` |
|     - | 4008 | `	unsigned char *zOut;` |
|     - | 4009 | `	int nLen,iLen;` |
|    50 | 4010 | `	sxu32 nOut = 0,nTotal = 0;` |
|    50 | 4011 | `	if( nArg < 1 ){` |
|     - | 4012 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 4013 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 4014 | `		return PH7_OK;` |
|     - | 4015 | `	}` |
|     - | 4016 | `	/* Extract the input string */` |
|    50 | 4017 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    50 | 4018 | `	if( nLen < 1 ){` |
|     - | 4019 | `		/* php refuses the empty string rather than decoding it to "". */` |
|     3 | 4020 | `		goto fail;` |
|     - | 4021 | `	}` |
|    48 | 4022 | `	zEnd = &zIn[nLen];` |
|     - | 4023 | `	/* Every four input characters yield three bytes and each line spends one more` |
|     - | 4024 | `	 * character on its length, so the input length is a safe upper bound. */` |
|    48 | 4025 | `	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nLen + 1);` |
|    48 | 4026 | `	if( zOut == 0 ){` |
|   ! 0 | 4027 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 4028 | `	}` |
|    72 | 4029 | `	while( zIn < zEnd ){` |
|    72 | 4030 | `		iLen = PH7_UU_DEC(*zIn++);` |
|    72 | 4031 | `		if( iLen == 0 ){` |
|     - | 4032 | `			/* The end marker (or any line claiming zero bytes) stops the decoding. */` |
|    12 | 4033 | `			break;` |
|     - | 4034 | `		}` |
|    62 | 4035 | `		if( iLen > nLen ){` |
|     5 | 4036 | `			goto err;` |
|     - | 4037 | `		}` |
|    58 | 4038 | `		nTotal += (sxu32)iLen;` |
|     - | 4039 | `		/* A line carries four characters per three-byte group, whole groups only. */` |
|    58 | 4040 | `		zStop = zIn + ((iLen + 2)/3)*4;` |
|    58 | 4041 | `		if( zStop > zEnd ){` |
|     5 | 4042 | `			goto err;` |
|     - | 4043 | `		}` |
|   544 | 4044 | `		while( zIn < zStop ){` |
|   492 | 4045 | `			zOut[nOut++] = (unsigned char)((PH7_UU_DEC(zIn[0]) << 2) \| (PH7_UU_DEC(zIn[1]) >> 4));` |
|   492 | 4046 | `			zOut[nOut++] = (unsigned char)((PH7_UU_DEC(zIn[1]) << 4) \| (PH7_UU_DEC(zIn[2]) >> 2));` |
|   492 | 4047 | `			zOut[nOut++] = (unsigned char)((PH7_UU_DEC(zIn[2]) << 6) \|  PH7_UU_DEC(zIn[3]));` |
|   492 | 4048 | `			zIn += 4;` |
|     2 | 4049 | `		}` |
|    54 | 4050 | `		if( iLen < 45 ){` |
|     - | 4051 | `			/* A short line ends the payload; whatever follows is ignored. */` |
|    30 | 4052 | `			break;` |
|     - | 4053 | `		}` |
|    25 | 4054 | `		zIn++; /* Skip the line separator */` |
|     1 | 4055 | `	}` |
|     - | 4056 | `	/* Drop the padding the last group carried: php keeps only as many bytes as the` |
|     - | 4057 | `	 * length bytes declared, counted over the WHOLE input rather than per line. */` |
|    40 | 4058 | `	if( nOut > nTotal ){` |
|    20 | 4059 | `		nOut = nTotal;` |
|     9 | 4060 | `	}` |
|    40 | 4061 | `	ph7_result_string(pCtx,(const char *)zOut,(int)nOut);` |
|    40 | 4062 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|    40 | 4063 | `	return PH7_OK;` |
|     4 | 4064 | `err:` |
|     9 | 4065 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|     5 | 4066 | `fail:` |
|    11 | 4067 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - | 4068 | `		"Argument #1 ($data) is not a valid uuencoded string"); /* the "convert_uudecode(): " prefix is added by the handler */` |
|    11 | 4069 | `	ph7_result_bool(pCtx,0);` |
|    11 | 4070 | `	return PH7_OK;` |
|    26 | 4071 | `}` |
|     - | 4072 | `/*` |
|     - | 4073 | ` * string urlencode(string $str)` |
|     - | 4074 | ` *  URL encoding` |
|     - | 4075 | ` * Parameter` |
|     - | 4076 | ` *  $data` |
|     - | 4077 | ` *   Input string.` |
|     - | 4078 | ` * Return` |
|     - | 4079 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|     - | 4080 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|     - | 4081 | ` *  encoded as plus (+) signs.` |
|     - | 4082 | ` */` |
|    16 | 4083 | `PH7_PRIVATE int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 4084 | `{` |
|     - | 4085 | `	const char *zIn;` |
|     - | 4086 | `	int nLen;` |
|    19 | 4087 | `	if( nArg < 1 ){` |
|     - | 4088 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 4089 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 4090 | `		return PH7_OK;` |
|     - | 4091 | `	}` |
|     - | 4092 | `	/* Extract the input string */` |
|    19 | 4093 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    19 | 4094 | `	if( nLen < 1 ){` |
|     - | 4095 | `		/* php returns an empty string for empty input, not FALSE */` |
|     6 | 4096 | `		ph7_result_string(pCtx,"",0);` |
|     6 | 4097 | `		return PH7_OK;` |
|     - | 4098 | `	}` |
|     - | 4099 | `	/* Perform the URL encoding */` |
|    14 | 4100 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|    14 | 4101 | `	return PH7_OK;` |
|    11 | 4102 | `}` |
|     - | 4103 | `/*` |
|     - | 4104 | ` * string rawurlencode(string $str)` |
|     - | 4105 | ` *  RFC 3986 URL encoding: spaces become %20 (not '+') and '~' is left intact.` |
|     - | 4106 | ` */` |
|    14 | 4107 | `PH7_PRIVATE int PH7_builtin_rawurlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 4108 | `{` |
|     - | 4109 | `	const char *zIn;` |
|     - | 4110 | `	int nLen;` |
|    17 | 4111 | `	if( nArg < 1 ){` |
|     - | 4112 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 4113 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 4114 | `		return PH7_OK;` |
|     - | 4115 | `	}` |
|     - | 4116 | `	/* Extract the input string */` |
|    17 | 4117 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 4118 | `	if( nLen < 1 ){` |
|     - | 4119 | `		/* php returns an empty string for empty input, not FALSE */` |
|     6 | 4120 | `		ph7_result_string(pCtx,"",0);` |
|     6 | 4121 | `		return PH7_OK;` |
|     - | 4122 | `	}` |
|     - | 4123 | `	/* Perform the RFC 3986 URL encoding */` |
|    12 | 4124 | `	SyUriEncodeRaw(zIn,(sxu32)nLen,Consumer,pCtx);` |
|    12 | 4125 | `	return PH7_OK;` |
|    10 | 4126 | `}` |
|     - | 4127 | `/* SyUriEncode/SyUriDecode write through a consumer; both query-string builtins` |
|     - | 4128 | ` * below want the bytes in a blob. */` |
|  5130 | 4129 | `static int UriBlobConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|     2 | 4130 | `{` |
|  5132 | 4131 | `	return (int)SyBlobAppend((SyBlob *)pUserData,pData,(sxu32)nLen);` |
|     2 | 4132 | `}` |
|     - | 4133 | `/* --- parse_str (php's main/php_variables.c) ---------------------------- */` |
|     - | 4134 |  |
|     - | 4135 | `/*` |
|     - | 4136 | ` * php_register_variable_ex(): register ONE decoded "name[idx][idx]" against a` |
|     - | 4137 | ` * target array. The name arrives ALREADY url-decoded, which is the rule the` |
|     - | 4138 | ` * chunk did not have -- php decodes the whole key first and only then looks for` |
|     - | 4139 | ` * brackets, so "a%5Bb%5D=1" is the NESTED a[b], not a flat key spelled "a[b]".` |
|     - | 4140 | ` *` |
|     - | 4141 | ` * The walk is destructive on its own copy of the name (php writes NULs over the` |
|     - | 4142 | ` * brackets), so zVar must be a writable NUL-terminated buffer.` |
|     - | 4143 | ` */` |
|   346 | 4144 | `static ph7_hashmap * ParseStrDescend(ph7_context *pCtx,ph7_hashmap *pMap,` |
|     - | 4145 | `	const char *zKey,ph7_value *pKey)` |
|     1 | 4146 | `{` |
|   347 | 4147 | `	ph7_hashmap_node *pNode = 0;` |
|     - | 4148 | `	ph7_value *pSlot,*pEmpty;` |
|   347 | 4149 | `	if( zKey ){` |
|   341 | 4150 | `		ph7_value_reset_string_cursor(pKey);` |
|   341 | 4151 | `		ph7_value_string(pKey,zKey,(int)SyStrlen(zKey));` |
|   341 | 4152 | `		if( PH7_HashmapLookup(pMap,pKey,&pNode) == SXRET_OK ){` |
|    27 | 4153 | `			pSlot = HashmapExtractNodeValue(pNode);` |
|    27 | 4154 | `			if( pSlot && (pSlot->iFlags & MEMOBJ_HASHMAP) ){` |
|    23 | 4155 | `				return (ph7_hashmap *)pSlot->x.pOther;` |
|     - | 4156 | `			}` |
|     2 | 4157 | `		}` |
|   159 | 4158 | `	}` |
|     - | 4159 | `	/* Nothing usable there: php OVERWRITES whatever scalar is in the way with a` |
|     - | 4160 | `	 * fresh array ("a=1&a[b]=2" ends as a['b']). */` |
|   325 | 4161 | `	pEmpty = ph7_context_new_array(pCtx);` |
|   325 | 4162 | `	if( pEmpty == 0 \|\| PH7_HashmapInsert(pMap,zKey ? pKey : 0,pEmpty) != SXRET_OK ){` |
|   ! 0 | 4163 | `		return 0;` |
|     - | 4164 | `	}` |
|   325 | 4165 | `	if( zKey ){` |
|   319 | 4166 | `		if( PH7_HashmapLookup(pMap,pKey,&pNode) != SXRET_OK ){` |
|   ! 0 | 4167 | `			return 0;` |
|     - | 4168 | `		}` |
|   160 | 4169 | `	}else{` |
|     7 | 4170 | `		pNode = pMap->pLast;   /* the append just made */` |
|     - | 4171 | `	}` |
|   325 | 4172 | `	pSlot = pNode ? HashmapExtractNodeValue(pNode) : 0;` |
|   325 | 4173 | `	return (pSlot && (pSlot->iFlags & MEMOBJ_HASHMAP)) ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|   174 | 4174 | `}` |
|  2156 | 4175 | `static void ParseStrRegister(ph7_context *pCtx,ph7_value *pTarget,char *zVar,` |
|     - | 4176 | `	ph7_value *pVal,int nMaxNest)` |
|     2 | 4177 | `{` |
|  2158 | 4178 | `	ph7_hashmap *pCur = (ph7_hashmap *)pTarget->x.pOther;` |
|     - | 4179 | `	ph7_value *pIdxKey;` |
|  2158 | 4180 | `	char *p,*ip = 0,*index;` |
|  2158 | 4181 | `	int bIsArray = 0,nNest = 0;` |
|     - | 4182 | `	/* php ignores leading SPACES in the name outright -- they are not mangled to` |
|     - | 4183 | `	 * '_' the way an interior space is. */` |
|  2164 | 4184 | `	while( zVar[0] == ' ' ){` |
|     7 | 4185 | `		zVar++;` |
|     1 | 4186 | `	}` |
|     - | 4187 | `	/* Neither a space nor a dot may live in a php variable name; both become '_'.` |
|     - | 4188 | `	 * The scan stops at the first '[', so only the BASE name is mangled. */` |
| 10210 | 4189 | `	for( p = zVar ; p[0] ; p++ ){` |
|  8134 | 4190 | `		if( p[0] == ' ' \|\| p[0] == '.' ){` |
|    20 | 4191 | `			p[0] = '_';` |
|  8125 | 4192 | `		}else if( p[0] == '[' ){` |
|    81 | 4193 | `			bIsArray = 1;` |
|    81 | 4194 | `			ip = p;` |
|    81 | 4195 | `			p[0] = 0;` |
|    81 | 4196 | `			break;` |
|     - | 4197 | `		}` |
|  4028 | 4198 | `	}` |
|  2158 | 4199 | `	if( p == zVar ){` |
|     5 | 4200 | `		return; /* empty name (or a name that was nothing but a space) */` |
|     - | 4201 | `	}` |
|  2154 | 4202 | `	index = zVar;` |
|  2154 | 4203 | `	pIdxKey = ph7_context_new_scalar(pCtx);` |
|  2154 | 4204 | `	if( pIdxKey == 0 ){` |
|   ! 0 | 4205 | `		return;` |
|     - | 4206 | `	}` |
|  2428 | 4207 | `	while( bIsArray ){` |
|     - | 4208 | `		char *zSeg;` |
|     - | 4209 | `		ph7_hashmap *pNext;` |
|   353 | 4210 | `		if( ++nNest > nMaxNest ){` |
|     - | 4211 | `			/* php drops the whole top-level variable it was building and warns.` |
|     - | 4212 | `			 * The message is deliberately vague about the input -- php calls` |
|     - | 4213 | `			 * saying more "information disclosure". */` |
|     3 | 4214 | `			ph7_hashmap_node *pNode = 0;` |
|     3 | 4215 | `			ph7_hashmap *pRoot = (ph7_hashmap *)pTarget->x.pOther;` |
|     3 | 4216 | `			ph7_value_reset_string_cursor(pIdxKey);` |
|     3 | 4217 | `			ph7_value_string(pIdxKey,zVar,(int)SyStrlen(zVar));` |
|     3 | 4218 | `			if( PH7_HashmapLookup(pRoot,pIdxKey,&pNode) == SXRET_OK ){` |
|     3 | 4219 | `				PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     1 | 4220 | `			}` |
|     4 | 4221 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - | 4222 | `				"Input variable nesting level exceeded %d. To increase the limit "` |
|     1 | 4223 | `				"change max_input_nesting_level in php.ini.",nMaxNest);` |
|     3 | 4224 | `			return;` |
|     - | 4225 | `		}` |
|   351 | 4226 | `		ip++;` |
|   351 | 4227 | `		zSeg = ip;` |
|   351 | 4228 | `		if( ip[0] == ' ' \|\| ip[0] == '\t' \|\| ip[0] == '\n' \|\| ip[0] == '\r' ){` |
|     5 | 4229 | `			ip++;   /* php skips ONE leading space before testing for ']' */` |
|     2 | 4230 | `		}` |
|   351 | 4231 | `		if( ip[0] == ']' ){` |
|    39 | 4232 | `			zSeg = 0;   /* "[]" (and "[ ]") appends */` |
|    20 | 4233 | `		}else{` |
|   651 | 4234 | `			while( ip[0] && ip[0] != ']' ){ ip++; }` |
|   313 | 4235 | `			if( ip[0] == 0 ){` |
|     - | 4236 | `				/* An unterminated '[': php un-terminates the name -- the bracket` |
|     - | 4237 | `				 * itself becomes '_' -- and the rest is mangled and used as a` |
|     - | 4238 | `				 * PLAIN key, so "a[b=1" registers "a_b". */` |
|     5 | 4239 | `				zSeg[-1] = '_';` |
|     7 | 4240 | `				for( p = zSeg ; p[0] ; p++ ){` |
|     3 | 4241 | `					if( p[0] == ' ' \|\| p[0] == '.' \|\| p[0] == '[' ){` |
|   ! 0 | 4242 | `						p[0] = '_';` |
|   ! 0 | 4243 | `					}` |
|     2 | 4244 | `				}` |
|     5 | 4245 | `				break;` |
|     - | 4246 | `			}` |
|   309 | 4247 | `			ip[0] = 0;` |
|     - | 4248 | `		}` |
|   347 | 4249 | `		pNext = ParseStrDescend(pCtx,pCur,index,pIdxKey);` |
|   347 | 4250 | `		if( pNext == 0 ){` |
|   ! 0 | 4251 | `			return;` |
|     - | 4252 | `		}` |
|   347 | 4253 | `		pCur = pNext;` |
|   347 | 4254 | `		index = zSeg;` |
|   347 | 4255 | `		ip++;` |
|   347 | 4256 | `		if( ip[0] == '[' ){` |
|   275 | 4257 | `			ip[0] = 0;   /* another level follows */` |
|   138 | 4258 | `		}else{` |
|    73 | 4259 | `			break;       /* whatever trails the last ']' is ignored */` |
|     - | 4260 | `		}` |
|     1 | 4261 | `	}` |
|  2152 | 4262 | `	if( index == 0 ){` |
|    33 | 4263 | `		PH7_HashmapInsert(pCur,0,pVal);` |
|    17 | 4264 | `	}else{` |
|  2120 | 4265 | `		ph7_value_reset_string_cursor(pIdxKey);` |
|  2120 | 4266 | `		ph7_value_string(pIdxKey,index,(int)SyStrlen(index));` |
|  2120 | 4267 | `		PH7_HashmapInsert(pCur,pIdxKey,pVal);` |
|     - | 4268 | `	}` |
|  1080 | 4269 | `}` |
|     - | 4270 | `/*` |
|     - | 4271 | ` * void parse_str(string $string, array &$result)` |
|     - | 4272 | ` *  Parse a query string into $result the way php's own GET/POST parser does.` |
|     - | 4273 | ` */` |
|   106 | 4274 | `PH7_PRIVATE int PH7_builtin_parse_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 4275 | `{` |
|     - | 4276 | `	ph7_value *pArray,*pVal;` |
|     - | 4277 | `	SyBlob sSep,sName,sValue;` |
|     - | 4278 | `	const char *zIn,*zSep;` |
|     - | 4279 | `	int nByte,nSep;` |
|   108 | 4280 | `	sxu32 i = 0;` |
|   108 | 4281 | `	sxi64 nCount = 0,nMaxVars,nMaxNest;` |
|   108 | 4282 | `	if( nArg < 2 ){` |
|     - | 4283 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|   ! 0 | 4284 | `		return PH7_OK;` |
|     - | 4285 | `	}` |
|   108 | 4286 | `	pArray = ph7_context_new_array(pCtx);` |
|   108 | 4287 | `	pVal = ph7_context_new_scalar(pCtx);` |
|   108 | 4288 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|   ! 0 | 4289 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 4290 | `	}` |
|   108 | 4291 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   108 | 4292 | `	nMaxVars = PH7_VmIniGetInt(pCtx->pVm,"max_input_vars",1000);` |
|   108 | 4293 | `	nMaxNest = PH7_VmIniGetInt(pCtx->pVm,"max_input_nesting_level",64);` |
|   108 | 4294 | `	SyBlobInit(&sSep,&pCtx->pVm->sAllocator);` |
|   108 | 4295 | `	SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|   108 | 4296 | `	SyBlobInit(&sValue,&pCtx->pVm->sAllocator);` |
|   108 | 4297 | `	PH7_VmIniGetStr(pCtx->pVm,"arg_separator.input",&sSep);` |
|   108 | 4298 | `	if( SyBlobLength(&sSep) < 1 ){` |
|   ! 0 | 4299 | `		SyBlobAppend(&sSep,"&",sizeof(char));` |
|   ! 0 | 4300 | `	}` |
|   108 | 4301 | `	zSep = (const char *)SyBlobData(&sSep);` |
|   108 | 4302 | `	nSep = (int)SyBlobLength(&sSep);` |
|     - | 4303 | `	/* php tokenizes with strtok(), so the separator is a SET of bytes and a run` |
|     - | 4304 | `	 * of them yields no empty field -- and an embedded NUL ends the input. */` |
|  2264 | 4305 | `	while( i < (sxu32)nByte && zIn[i] ){` |
|     - | 4306 | `		sxu32 iStart,iEq;` |
|     - | 4307 | `		int bFound;` |
|  4224 | 4308 | `		while( i < (sxu32)nByte && zIn[i] ){` |
|     - | 4309 | `			int s;` |
|  6380 | 4310 | `			for( s = 0 ; s < nSep ; ++s ){` |
|  4222 | 4311 | `				if( zIn[i] == zSep[s] ){ break; }` |
|  1081 | 4312 | `			}` |
|  4222 | 4313 | `			if( s == nSep ){ break; }` |
|  2064 | 4314 | `			i++;` |
|     2 | 4315 | `		}` |
|  2162 | 4316 | `		if( i >= (sxu32)nByte \|\| zIn[i] == 0 ){` |
|     2 | 4317 | `			break;` |
|     - | 4318 | `		}` |
|  2160 | 4319 | `		iStart = i;` |
|  2160 | 4320 | `		iEq = 0;` |
|  2160 | 4321 | `		bFound = 0;` |
| 19532 | 4322 | `		while( i < (sxu32)nByte && zIn[i] ){` |
|     - | 4323 | `			int s;` |
| 36806 | 4324 | `			for( s = 0 ; s < nSep ; ++s ){` |
| 19434 | 4325 | `				if( zIn[i] == zSep[s] ){ break; }` |
|  8688 | 4326 | `			}` |
| 19434 | 4327 | `			if( s < nSep ){ break; }` |
| 17374 | 4328 | `			if( zIn[i] == '=' && !bFound ){` |
|  2152 | 4329 | `				iEq = i;` |
|  2152 | 4330 | `				bFound = 1;` |
|  1075 | 4331 | `			}` |
| 17374 | 4332 | `			i++;` |
|     2 | 4333 | `		}` |
|  2160 | 4334 | `		if( ++nCount > nMaxVars ){` |
|     4 | 4335 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - | 4336 | `				"Input variables exceeded %qd. To increase the limit change "` |
|     1 | 4337 | `				"max_input_vars in php.ini.",nMaxVars);` |
|     3 | 4338 | `			break;` |
|     - | 4339 | `		}` |
|     - | 4340 | `		/* Both halves are url-decoded BEFORE the name is parsed for brackets. */` |
|  2158 | 4341 | `		SyBlobReset(&sName);` |
|  2158 | 4342 | `		SyBlobReset(&sValue);` |
|  2158 | 4343 | `		if( bFound ){` |
|  2150 | 4344 | `			if( iEq > iStart ){` |
|  2148 | 4345 | `				SyUriDecode(&zIn[iStart],iEq - iStart,UriBlobConsumer,&sName,TRUE);` |
|  1073 | 4346 | `			}` |
|  2150 | 4347 | `			if( i > iEq + 1 ){` |
|  2148 | 4348 | `				SyUriDecode(&zIn[iEq + 1],i - (iEq + 1),UriBlobConsumer,&sValue,TRUE);` |
|  1073 | 4349 | `			}` |
|  1076 | 4350 | `		}else{` |
|     9 | 4351 | `			SyUriDecode(&zIn[iStart],i - iStart,UriBlobConsumer,&sName,TRUE);` |
|     - | 4352 | `		}` |
|  2158 | 4353 | `		SyBlobAppend(&sName,"\0",sizeof(char));   /* the walk is C-string based */` |
|  2158 | 4354 | `		ph7_value_string(pVal,(const char *)SyBlobData(&sValue),(int)SyBlobLength(&sValue));` |
|  2158 | 4355 | `		ParseStrRegister(pCtx,pArray,(char *)SyBlobData(&sName),pVal,(int)nMaxNest);` |
|  2158 | 4356 | `		ph7_value_reset_string_cursor(pVal);` |
|     2 | 4357 | `	}` |
|   108 | 4358 | `	SyBlobRelease(&sSep);` |
|   108 | 4359 | `	SyBlobRelease(&sName);` |
|   108 | 4360 | `	SyBlobRelease(&sValue);` |
|     - | 4361 | `	/* $result is by REFERENCE and php REPLACES it, empty array included. */` |
|   108 | 4362 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pArray);` |
|   108 | 4363 | `	return PH7_OK;` |
|    55 | 4364 | `}` |
|     - | 4365 | `/* --- http_build_query (php's ext/standard/http.c) ---------------------- */` |
|     - | 4366 |  |
|     - | 4367 | `/*` |
|     - | 4368 | ` * The chain of hashmaps and instances the walk is currently INSIDE. This is` |
|     - | 4369 | ` * php's GC_TRY_PROTECT_RECURSION without a mark bit: a container that is its own` |
|     - | 4370 | `` * ancestor contributes nothing, so `$a['self'] = &$a` builds "a=1" rather than`` |
|     - | 4371 | ` * recursing forever. PHL had no guard here at all and ran the allocator out of` |
|     - | 4372 | ` * memory on exactly that input.` |
|     - | 4373 | ` */` |
|     - | 4374 | `typedef struct http_query_frame http_query_frame;` |
|     - | 4375 | `struct http_query_frame {` |
|     - | 4376 | `	const void *pWalked;                  /* the ph7_hashmap / ph7_class_instance */` |
|     - | 4377 | `	const http_query_frame *pParent;` |
|     - | 4378 | `};` |
|     - | 4379 | `typedef struct http_query_state http_query_state;` |
|     - | 4380 | `struct http_query_state {` |
|     - | 4381 | `	ph7_context *pCtx;` |
|     - | 4382 | `	SyBlob *pOut;      /* the form string built so far */` |
|     - | 4383 | `	const char *zSep;  /* argument separator */` |
|     - | 4384 | `	sxu32 nSep;` |
|     - | 4385 | `	int bRaw;          /* PHP_QUERY_RFC3986 rather than RFC1738 */` |
|     - | 4386 | `	int nDepth;` |
|     - | 4387 | `	int rc;            /* PH7_OK, or the status of a throw in flight */` |
|     - | 4388 | `};` |
|     - | 4389 | `/*` |
|     - | 4390 | ` * php has no fixed nesting limit here -- it asks the platform whether the C` |
|     - | 4391 | ` * stack is nearly gone and throws "Maximum call stack size reached." when it is.` |
|     - | 4392 | ` * PHL walks the same tree on the same C stack, so it needs a bound; this one is` |
|     - | 4393 | ` * far above any query string anyone builds and reports php's own error.` |
|     - | 4394 | ` */` |
|     - | 4395 | `#define HTTP_QUERY_MAX_DEPTH 512` |
|     - | 4396 |  |
|   696 | 4397 | `static int HttpQueryIsAncestor(const http_query_frame *pFrame,const void *pWalked)` |
|     1 | 4398 | `{` |
| 90431 | 4399 | `	while( pFrame ){` |
| 89739 | 4400 | `		if( pFrame->pWalked == pWalked ){` |
|     5 | 4401 | `			return 1;` |
|     - | 4402 | `		}` |
| 89735 | 4403 | `		pFrame = pFrame->pParent;` |
|     1 | 4404 | `	}` |
|   693 | 4405 | `	return 0;` |
|   349 | 4406 | `}` |
|   780 | 4407 | `static void HttpQueryEncodeTo(SyBlob *pOut,int bRaw,const char *zIn,sxu32 nByte)` |
|     1 | 4408 | `{` |
|   781 | 4409 | `	if( nByte < 1 ){` |
|   ! 0 | 4410 | `		return;` |
|     - | 4411 | `	}` |
|   781 | 4412 | `	if( bRaw ){` |
|     5 | 4413 | `		SyUriEncodeRaw(zIn,nByte,UriBlobConsumer,pOut);` |
|     3 | 4414 | `	}else{` |
|   777 | 4415 | `		SyUriEncode(zIn,nByte,UriBlobConsumer,pOut);` |
|     - | 4416 | `	}` |
|   391 | 4417 | `}` |
|     - | 4418 | `static int HttpQueryWalk(http_query_state *p,ph7_value *pData,` |
|     - | 4419 | `	const char *zNumPrefix,sxu32 nNumPrefix,` |
|     - | 4420 | `	const char *zKeyPrefix,sxu32 nKeyPrefix,` |
|     - | 4421 | `	const http_query_frame *pParent);` |
|     - | 4422 |  |
|     - | 4423 | `/*` |
|     - | 4424 | ` * php_url_encode_scalar(): one "<key_prefix><key>[%5D]=<value>" leaf, preceded` |
|     - | 4425 | ` * by the separator once anything has been written.` |
|     - | 4426 | ` */` |
|   106 | 4427 | `static void HttpQueryScalar(http_query_state *p,` |
|     - | 4428 | `	int bIntKey,sxi64 iKey,const char *zKey,sxu32 nKey,` |
|     - | 4429 | `	ph7_value *pVal,` |
|     - | 4430 | `	const char *zNumPrefix,sxu32 nNumPrefix,` |
|     - | 4431 | `	const char *zKeyPrefix,sxu32 nKeyPrefix)` |
|     1 | 4432 | `{` |
|   107 | 4433 | `	if( SyBlobLength(p->pOut) > 0 ){` |
|    49 | 4434 | `		SyBlobAppend(p->pOut,p->zSep,p->nSep);` |
|    24 | 4435 | `	}` |
|   107 | 4436 | `	if( nKeyPrefix > 0 ){` |
|    45 | 4437 | `		SyBlobAppend(p->pOut,zKeyPrefix,nKeyPrefix);` |
|    22 | 4438 | `	}` |
|   107 | 4439 | `	if( bIntKey ){` |
|     - | 4440 | `		/* The numeric prefix is appended RAW -- php never url-encodes it, which` |
|     - | 4441 | `		 * is why http_build_query([1,2], "a b") answers "a b0=1&a b1=2". The` |
|     - | 4442 | `		 * chunk encoded it and answered "a+b0=1". */` |
|    53 | 4443 | `		if( nNumPrefix > 0 ){` |
|    17 | 4444 | `			SyBlobAppend(p->pOut,zNumPrefix,nNumPrefix);` |
|     8 | 4445 | `		}` |
|    53 | 4446 | `		SyBlobFormat(p->pOut,"%qd",iKey);` |
|    27 | 4447 | `	}else{` |
|    55 | 4448 | `		HttpQueryEncodeTo(p->pOut,p->bRaw,zKey,nKey);` |
|     - | 4449 | `	}` |
|   107 | 4450 | `	if( nKeyPrefix > 0 ){` |
|    45 | 4451 | `		SyBlobAppend(p->pOut,"%5D",sizeof("%5D")-1);` |
|    22 | 4452 | `	}` |
|   107 | 4453 | `	SyBlobAppend(p->pOut,"=",sizeof(char));` |
|   107 | 4454 | `	if( ph7_value_is_bool(pVal) ){` |
|     - | 4455 | `		/* php writes the digit itself: to_string() would give "" for false. */` |
|     5 | 4456 | `		SyBlobAppend(p->pOut,ph7_value_to_bool(pVal) ? "1" : "0",sizeof(char));` |
|     3 | 4457 | `	}else{` |
|     - | 4458 | `		int nVal;` |
|   103 | 4459 | `		const char *zVal = ph7_value_to_string(pVal,&nVal);` |
|   103 | 4460 | `		HttpQueryEncodeTo(p->pOut,p->bRaw,zVal,(sxu32)nVal);` |
|     - | 4461 | `	}` |
|   107 | 4462 | `}` |
|     - | 4463 | `/*` |
|     - | 4464 | ` * Build the key prefix a nested container's members carry: php closes the` |
|     - | 4465 | ` * PREVIOUS bracket and opens the next one in the same step, so a second level` |
|     - | 4466 | ` * appends "%5D%5B" where the first opened with "%5B".` |
|     - | 4467 | ` */` |
|   634 | 4468 | `static void HttpQueryNestPrefix(http_query_state *p,SyBlob *pPrefix,` |
|     - | 4469 | `	int bIntKey,sxi64 iKey,const char *zKey,sxu32 nKey,` |
|     - | 4470 | `	const char *zNumPrefix,sxu32 nNumPrefix,` |
|     - | 4471 | `	const char *zKeyPrefix,sxu32 nKeyPrefix)` |
|     1 | 4472 | `{` |
|   635 | 4473 | `	if( nKeyPrefix > 0 ){` |
|   601 | 4474 | `		SyBlobAppend(pPrefix,zKeyPrefix,nKeyPrefix);` |
|   335 | 4475 | `	}else if( bIntKey && nNumPrefix > 0 ){` |
|     9 | 4476 | `		SyBlobAppend(pPrefix,zNumPrefix,nNumPrefix);` |
|     4 | 4477 | `	}` |
|   635 | 4478 | `	if( bIntKey ){` |
|    11 | 4479 | `		SyBlobFormat(pPrefix,"%qd",iKey);` |
|     6 | 4480 | `	}else{` |
|   625 | 4481 | `		HttpQueryEncodeTo(pPrefix,p->bRaw,zKey,nKey);` |
|     - | 4482 | `	}` |
|   952 | 4483 | `	SyBlobAppend(pPrefix,nKeyPrefix > 0 ? "%5D%5B" : "%5B",` |
|   317 | 4484 | `		nKeyPrefix > 0 ? sizeof("%5D%5B")-1 : sizeof("%5B")-1);` |
|   635 | 4485 | `}` |
|     - | 4486 | `/*` |
|     - | 4487 | ` * One (key, value) pair, whichever container it came from. php skips NULL and` |
|     - | 4488 | ` * RESOURCE outright, descends into an array or a non-enum object, and treats` |
|     - | 4489 | ` * everything else -- a backed enum case included -- as a scalar.` |
|     - | 4490 | ` */` |
|   748 | 4491 | `static void HttpQueryPair(http_query_state *p,` |
|     - | 4492 | `	int bIntKey,sxi64 iKey,const char *zKey,sxu32 nKey,` |
|     - | 4493 | `	ph7_value *pVal,` |
|     - | 4494 | `	const char *zNumPrefix,sxu32 nNumPrefix,` |
|     - | 4495 | `	const char *zKeyPrefix,sxu32 nKeyPrefix,` |
|     - | 4496 | `	const http_query_frame *pParent)` |
|     1 | 4497 | `{` |
|     - | 4498 | `	int bDescend;` |
|   749 | 4499 | `	if( p->rc != PH7_OK ){` |
|   ! 0 | 4500 | `		return;` |
|     - | 4501 | `	}` |
|   749 | 4502 | `	if( ph7_value_is_null(pVal) \|\| ph7_value_is_resource(pVal) ){` |
|     7 | 4503 | `		return;` |
|     - | 4504 | `	}` |
|   743 | 4505 | `	bDescend = ph7_value_is_array(pVal);` |
|   743 | 4506 | `	if( ph7_value_is_object(pVal) ){` |
|    11 | 4507 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|    11 | 4508 | `		if( (pInst->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     5 | 4509 | `			bDescend = 1;` |
|     3 | 4510 | `		}else{` |
|     - | 4511 | `			/* php compares an enum case by its BACKING value here; the chunk` |
|     - | 4512 | `			 * descended into it and emitted its name/value properties. */` |
|     7 | 4513 | `			ph7_value *pBacking = PH7_EnumCaseBackingValueOf(pInst);` |
|     7 | 4514 | `			if( pBacking == 0 ){` |
|     5 | 4515 | `				p->rc = PH7_VmThrowException(p->pCtx,"ValueError",` |
|     - | 4516 | `					"Unbacked enum %z cannot be converted to a string",` |
|     2 | 4517 | `					&pInst->pClass->sName);` |
|     3 | 4518 | `				return;` |
|     - | 4519 | `			}` |
|     7 | 4520 | `			HttpQueryScalar(p,bIntKey,iKey,zKey,nKey,pBacking,` |
|     2 | 4521 | `				zNumPrefix,nNumPrefix,zKeyPrefix,nKeyPrefix);` |
|     5 | 4522 | `			return;` |
|     - | 4523 | `		}` |
|     2 | 4524 | `	}` |
|   737 | 4525 | `	if( bDescend ){` |
|     - | 4526 | `		SyBlob sPrefix;` |
|   635 | 4527 | `		SyBlobInit(&sPrefix,&p->pCtx->pVm->sAllocator);` |
|   952 | 4528 | `		HttpQueryNestPrefix(p,&sPrefix,bIntKey,iKey,zKey,nKey,` |
|   317 | 4529 | `			zNumPrefix,nNumPrefix,zKeyPrefix,nKeyPrefix);` |
|     - | 4530 | `		/* php passes no numeric prefix down: it only ever prefixes a TOP-LEVEL` |
|     - | 4531 | `		 * integer key. */` |
|   952 | 4532 | `		HttpQueryWalk(p,pVal,0,0,` |
|   634 | 4533 | `			(const char *)SyBlobData(&sPrefix),SyBlobLength(&sPrefix),pParent);` |
|   635 | 4534 | `		SyBlobRelease(&sPrefix);` |
|   635 | 4535 | `		return;` |
|     - | 4536 | `	}` |
|   154 | 4537 | `	HttpQueryScalar(p,bIntKey,iKey,zKey,nKey,pVal,` |
|    51 | 4538 | `		zNumPrefix,nNumPrefix,zKeyPrefix,nKeyPrefix);` |
|   375 | 4539 | `}` |
|     - | 4540 | `/* Every visible, non-static, materialized property of an instance, in` |
|     - | 4541 | ` * declaration order. php asks the CALLER's scope, so http_build_query($this)` |
|     - | 4542 | ` * from inside the class sees its private members -- the chunk reached them` |
|     - | 4543 | ` * through a global-scope get_object_vars() and never did. */` |
|     8 | 4544 | `static void HttpQueryWalkObject(http_query_state *p,ph7_class_instance *pThis,` |
|     - | 4545 | `	const char *zKeyPrefix,sxu32 nKeyPrefix,const http_query_frame *pFrame)` |
|     1 | 4546 | `{` |
|     - | 4547 | `	SyHashEntry *pEntry;` |
|     - | 4548 | `	ph7_value sValue;` |
|     9 | 4549 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|     9 | 4550 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    39 | 4551 | `	while( p->rc == PH7_OK && (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    31 | 4552 | `		VmClassAttr *pAttr = (VmClassAttr *)pEntry->pUserData;` |
|    31 | 4553 | `		SyString *pName = &pAttr->pAttr->sName;` |
|     - | 4554 | `		ph7_value *pValue;` |
|    31 | 4555 | `		if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|   ! 0 | 4556 | `			continue;` |
|     - | 4557 | `		}` |
|    31 | 4558 | `		if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     - | 4559 | `			/* A virtual hooked property has no backing store, and php reads the` |
|     - | 4560 | `			 * raw property table here rather than dispatching the get hook. */` |
|     5 | 4561 | `			continue;` |
|     - | 4562 | `		}` |
|    40 | 4563 | `		if( !PH7_VmClassMemberAccess(pThis->pVm,pThis->pClass,pName,` |
|    26 | 4564 | `				pAttr->pAttr->iProtection,FALSE) ){` |
|     7 | 4565 | `			continue;` |
|     - | 4566 | `		}` |
|    21 | 4567 | `		pValue = PH7_ClassInstanceExtractAttrValue(pThis,pAttr);` |
|    21 | 4568 | `		if( pValue == 0 ){` |
|   ! 0 | 4569 | `			continue;` |
|     - | 4570 | `		}` |
|    21 | 4571 | `		PH7_MemObjLoad(pValue,&sValue);` |
|    31 | 4572 | `		HttpQueryPair(p,0,0,SyStringData(pName),SyStringLength(pName),&sValue,` |
|    10 | 4573 | `			0,0,zKeyPrefix,nKeyPrefix,pFrame);` |
|    21 | 4574 | `		PH7_MemObjRelease(&sValue);` |
|     1 | 4575 | `	}` |
|     9 | 4576 | `	PH7_MemObjRelease(&sValue);` |
|     9 | 4577 | `}` |
|     - | 4578 | `/* php_url_encode_hash_ex() over one array or object. */` |
|   696 | 4579 | `static int HttpQueryWalk(http_query_state *p,ph7_value *pData,` |
|     - | 4580 | `	const char *zNumPrefix,sxu32 nNumPrefix,` |
|     - | 4581 | `	const char *zKeyPrefix,sxu32 nKeyPrefix,` |
|     - | 4582 | `	const http_query_frame *pParent)` |
|     1 | 4583 | `{` |
|     - | 4584 | `	http_query_frame sFrame;` |
|   697 | 4585 | `	const void *pWalked = pData->x.pOther;` |
|   697 | 4586 | `	if( HttpQueryIsAncestor(pParent,pWalked) ){` |
|     5 | 4587 | `		return PH7_OK;` |
|     - | 4588 | `	}` |
|   693 | 4589 | `	if( p->nDepth >= HTTP_QUERY_MAX_DEPTH ){` |
|   ! 0 | 4590 | `		p->rc = PH7_VmThrowException(p->pCtx,"Error","Maximum call stack size reached.");` |
|   ! 0 | 4591 | `		return p->rc;` |
|     - | 4592 | `	}` |
|   693 | 4593 | `	sFrame.pWalked = pWalked;` |
|   693 | 4594 | `	sFrame.pParent = pParent;` |
|   693 | 4595 | `	p->nDepth++;` |
|   693 | 4596 | `	if( ph7_value_is_object(pData) ){` |
|     9 | 4597 | `		HttpQueryWalkObject(p,(ph7_class_instance *)pWalked,zKeyPrefix,nKeyPrefix,&sFrame);` |
|     5 | 4598 | `	}else{` |
|   685 | 4599 | `		ph7_hashmap *pMap = (ph7_hashmap *)pWalked;` |
|   685 | 4600 | `		ph7_hashmap_node *pNode = pMap->pFirst;` |
|     - | 4601 | `		ph7_value sValue;` |
|   685 | 4602 | `		sxu32 n = pMap->nEntry;` |
|   685 | 4603 | `		PH7_MemObjInit(pMap->pVm,&sValue);` |
|     - | 4604 | `		/* Insertion order runs pFirst then the pPrev chain (MACRO_LD_PUSH links` |
|     - | 4605 | `		 * a new node in through pNext, so pNext is the OLDER neighbour). */` |
|  1413 | 4606 | `		while( n > 0 && p->rc == PH7_OK ){` |
|   729 | 4607 | `			int bIntKey = (pNode->iType == HASHMAP_INT_NODE);` |
|   729 | 4608 | `			PH7_HashmapExtractNodeValue(pNode,&sValue,FALSE);` |
|  1093 | 4609 | `			HttpQueryPair(p,bIntKey,bIntKey ? pNode->xKey.iKey : 0,` |
|   364 | 4610 | `				bIntKey ? 0 : (const char *)SyBlobData(&pNode->xKey.sKey),` |
|   364 | 4611 | `				bIntKey ? 0 : SyBlobLength(&pNode->xKey.sKey),` |
|   364 | 4612 | `				&sValue,zNumPrefix,nNumPrefix,zKeyPrefix,nKeyPrefix,&sFrame);` |
|   729 | 4613 | `			PH7_MemObjRelease(&sValue);` |
|   729 | 4614 | `			pNode = pNode->pPrev;` |
|   729 | 4615 | `			n--;` |
|     1 | 4616 | `		}` |
|   685 | 4617 | `		PH7_MemObjRelease(&sValue);` |
|     - | 4618 | `	}` |
|   693 | 4619 | `	p->nDepth--;` |
|   693 | 4620 | `	return p->rc;` |
|   349 | 4621 | `}` |
|     - | 4622 | `/*` |
|     - | 4623 | ` * string http_build_query(object\|array $data, string $numeric_prefix = "",` |
|     - | 4624 | ` *                         ?string $arg_separator = null,` |
|     - | 4625 | ` *                         int $encoding_type = PHP_QUERY_RFC1738)` |
|     - | 4626 | ` *  Generate a URL-encoded query string from an array or an object.` |
|     - | 4627 | ` */` |
|    76 | 4628 | `PH7_PRIVATE int PH7_builtin_http_build_query(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 4629 | `{` |
|     - | 4630 | `	http_query_state sState;` |
|     - | 4631 | `	SyBlob sOut;` |
|     - | 4632 | `	char zName[64];` |
|    78 | 4633 | `	const char *zNumPrefix = 0,*zSep = "&";` |
|    78 | 4634 | `	int nNumPrefix = 0,nSep = 1;` |
|    78 | 4635 | `	if( nArg < 1 ){` |
|     - | 4636 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|   ! 0 | 4637 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 4638 | `		return PH7_OK;` |
|     - | 4639 | `	}` |
|     - | 4640 | ``	/* php DECLARES `object\|array $data` and REPORTS "must be of type array" --`` |
|     - | 4641 | ``	 * the shared ZPP screen leaves a union arm holding `array` alone for exactly`` |
|     - | 4642 | `	 * this reason, so the wording is the builtin's own. */` |
|    78 | 4643 | `	if( !ph7_value_is_array(apArg[0]) && !ph7_value_is_object(apArg[0]) ){` |
|    16 | 4644 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 4645 | `			"http_build_query(): Argument #1 ($data) must be of type array, %s given",` |
|     5 | 4646 | `			VmValueGivenName(apArg[0],zName,sizeof(zName)));` |
|     - | 4647 | `	}` |
|    67 | 4648 | `	if( ph7_value_is_object(apArg[0]) ){` |
|    11 | 4649 | `		ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    11 | 4650 | `		if( pInst->pClass->iFlags & PH7_CLASS_ENUM ){` |
|     7 | 4651 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 4652 | `				"http_build_query(): Argument #1 ($data) must not be an enum, %z given",` |
|     4 | 4653 | `				&pInst->pClass->sName);` |
|     - | 4654 | `		}` |
|     3 | 4655 | `	}` |
|    63 | 4656 | `	if( nArg > 1 ){` |
|    33 | 4657 | `		zNumPrefix = ph7_value_to_string(apArg[1],&nNumPrefix);` |
|    16 | 4658 | `	}` |
|    63 | 4659 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|     3 | 4660 | `		zSep = ph7_value_to_string(apArg[2],&nSep);` |
|     1 | 4661 | `	}` |
|    63 | 4662 | `	sState.pCtx = pCtx;` |
|    63 | 4663 | `	sState.zSep = zSep;` |
|    63 | 4664 | `	sState.nSep = (sxu32)nSep;` |
|    63 | 4665 | `	sState.bRaw = (nArg > 3) && (ph7_value_to_int(apArg[3]) == 2 /* PHP_QUERY_RFC3986 */);` |
|    63 | 4666 | `	sState.nDepth = 0;` |
|    63 | 4667 | `	sState.rc = PH7_OK;` |
|    63 | 4668 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    63 | 4669 | `	sState.pOut = &sOut;` |
|    63 | 4670 | `	HttpQueryWalk(&sState,apArg[0],zNumPrefix,(sxu32)nNumPrefix,0,0,0);` |
|    63 | 4671 | `	if( sState.rc == PH7_OK ){` |
|    61 | 4672 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    30 | 4673 | `	}` |
|    63 | 4674 | `	SyBlobRelease(&sOut);` |
|    63 | 4675 | `	return sState.rc;` |
|    40 | 4676 | `}` |
|     - | 4677 | `/*` |
|     - | 4678 | ` * string urldecode(string $str)` |
|     - | 4679 | ` *  Decodes any %## encoding in the given string.` |
|     - | 4680 | ` *  Plus symbols ('+') are decoded to a space character.` |
|     - | 4681 | ` * string rawurldecode(string $str)` |
|     - | 4682 | ` *  The same, except that '+' is NOT a space: RFC 3986 has no plus convention, so` |
|     - | 4683 | ` *  php leaves it alone. rawurldecode() used to be registered as an ALIAS of` |
|     - | 4684 | ` *  urldecode(), which turned every literal '+' into a space.` |
|     - | 4685 | ` * Parameter` |
|     - | 4686 | ` *  $data` |
|     - | 4687 | ` *    Input string.` |
|     - | 4688 | ` * Return` |
|     - | 4689 | ` *  Decoded URL or FALSE on failure.` |
|     - | 4690 | ` */` |
|   120 | 4691 | `static int UrlDecodeCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPlus)` |
|     3 | 4692 | `{` |
|     - | 4693 | `	const char *zIn;` |
|     - | 4694 | `	int nLen;` |
|   123 | 4695 | `	if( nArg < 1 ){` |
|     - | 4696 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 4697 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 4698 | `		return PH7_OK;` |
|     - | 4699 | `	}` |
|     - | 4700 | `	/* Extract the input string */` |
|   123 | 4701 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|   123 | 4702 | `	if( nLen < 1 ){` |
|     - | 4703 | `		/* php returns an empty string for empty input, not FALSE */` |
|    13 | 4704 | `		ph7_result_string(pCtx,"",0);` |
|    13 | 4705 | `		return PH7_OK;` |
|     - | 4706 | `	}` |
|     - | 4707 | `	/* Perform the URL decoding */` |
|   112 | 4708 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,bPlus);` |
|   112 | 4709 | `	return PH7_OK;` |
|    63 | 4710 | `}` |
|    64 | 4711 | `PH7_PRIVATE int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 4712 | `{` |
|    67 | 4713 | `	return UrlDecodeCommon(pCtx,nArg,apArg,TRUE);` |
|     3 | 4714 | `}` |
|    56 | 4715 | `PH7_PRIVATE int PH7_builtin_rawurldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 4716 | `{` |
|    59 | 4717 | `	return UrlDecodeCommon(pCtx,nArg,apArg,FALSE);` |
|     3 | 4718 | `}` |
|     - | 4719 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|     - | 4720 |  |
