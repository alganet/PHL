# src/ph7/builtin_parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3235/3476 lines (93.07%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `#include <stdlib.h>  /* strtod */` |
|      - |    8 | `#include <math.h>    /* HUGE_VAL */` |
|      - |    9 | `#include <errno.h>   /* ERANGE (strtod range-error signal) */` |
|      - |   10 | `/*` |
|      - |   11 | ` * Section:` |
|      - |   12 | ` *    Parsing/classification functions: filter_var, CSV, strip_tags,` |
|      - |   13 | ` *    parse_ini_string, the ctype_* family and URL/base64 coding.` |
|      - |   14 | ` * Status:` |
|      - |   15 | ` *    Stable.` |
|      - |   16 | ` */` |
|      - |   17 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |   18 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |   19 | `#endif` |
|      - |   20 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |   21 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |   22 | `#endif` |
|      - |   23 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |   24 | `/*` |
|      - |   25 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - |   26 | ` *` |
|      - |   27 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - |   28 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - |   29 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - |   30 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - |   31 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - |   32 | ` */` |
|      - |   33 | `#define FV_VALIDATE_INT     257` |
|      - |   34 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - |   35 | `#define FV_VALIDATE_FLOAT   259` |
|      - |   36 | `#define FV_VALIDATE_REGEXP  272` |
|      - |   37 | `#define FV_VALIDATE_URL     273` |
|      - |   38 | `#define FV_VALIDATE_EMAIL   274` |
|      - |   39 | `#define FV_VALIDATE_IP      275` |
|      - |   40 | `#define FV_VALIDATE_MAC     276` |
|      - |   41 | `#define FV_VALIDATE_DOMAIN  277` |
|      - |   42 | `#define FV_SANITIZE_STRING             513` |
|      - |   43 | `#define FV_SANITIZE_ENCODED            514` |
|      - |   44 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - |   45 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - |   46 | `#define FV_SANITIZE_EMAIL   517` |
|      - |   47 | `#define FV_SANITIZE_URL     518` |
|      - |   48 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - |   49 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - |   50 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - |   51 | `#define FV_SANITIZE_ADD_SLASHES        523` |
|      - |   52 | `#define FV_CALLBACK                    1024` |
|      - |   53 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - |   54 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - |   55 | `#define FV_FLAG_STRIP_LOW    4` |
|      - |   56 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - |   57 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - |   58 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - |   59 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - |   60 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - |   61 | `#define FV_FLAG_EMPTY_STRING_NULL 256` |
|      - |   62 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - |   63 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - |   64 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - |   65 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - |   66 | `#define FV_FLAG_PATH_REQUIRED  262144` |
|      - |   67 | `#define FV_FLAG_QUERY_REQUIRED 524288` |
|      - |   68 | `#define FV_FLAG_IPV4  1048576` |
|      - |   69 | `/* php gives one bit three names, one per filter it belongs to. */` |
|      - |   70 | `#define FV_FLAG_HOSTNAME      1048576` |
|      - |   71 | `#define FV_FLAG_EMAIL_UNICODE 1048576` |
|      - |   72 | `#define FV_FLAG_IPV6  2097152` |
|      - |   73 | `#define FV_FLAG_NO_RES_RANGE  4194304` |
|      - |   74 | `#define FV_FLAG_NO_PRIV_RANGE 8388608` |
|      - |   75 | `#define FV_FLAG_GLOBAL_RANGE  536870912` |
|      - |   76 | `#define FV_REQUIRE_ARRAY   16777216` |
|      - |   77 | `#define FV_REQUIRE_SCALAR  33554432` |
|      - |   78 | `#define FV_FORCE_ARRAY     67108864` |
|      - |   79 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - |   80 | `#define FV_THROW_ON_FAILURE 268435456` |
|      - |   81 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - |   82 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - |   83 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - |   84 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - |   85 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - |   86 |  |
|      - |   87 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - |   88 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    323 |   89 | `static void FvTrim(const char **pz,int *pn){` |
|    323 |   90 | `	const char *z = *pz;` |
|    323 |   91 | `	int n = *pn;` |
|    327 |   92 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    331 |   93 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    323 |   94 | `	*pz = z; *pn = n;` |
|    323 |   95 | `}` |
|      - |   96 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|    161 |   97 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|    161 |   98 | `	int neg = 0, i;` |
|    161 |   99 | `	sxu64 u = 0;` |
|    161 |  100 | `	FvTrim(&z,&n);` |
|    161 |  101 | `	if( n==0 ){ return 0; }` |
|    149 |  102 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|    149 |  103 | `	if( n==0 ){ return 0; }` |
|    147 |  104 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 |  105 | `		z += 2; n -= 2;` |
|      3 |  106 | `		if( n==0 ){ return 0; }` |
|      7 |  107 | `		for( i=0; i<n; i++ ){` |
|      5 |  108 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 |  109 | `			if( h<0 ){ return 0; }` |
|      5 |  110 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 |  111 | `			u = u*16 + (sxu64)h;` |
|      3 |  112 | `		}` |
|    144 |  113 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 |  114 | `		for( i=0; i<n; i++ ){` |
|      7 |  115 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 |  116 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 |  117 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 |  118 | `		}` |
|      2 |  119 | `	}else{` |
|    143 |  120 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    381 |  121 | `		for( i=0; i<n; i++ ){` |
|    283 |  122 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    245 |  123 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    245 |  124 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|    124 |  125 | `		}` |
|      - |  126 | `	}` |
|    105 |  127 | `	if( neg ){` |
|      5 |  128 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 |  129 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 |  130 | `	}else{` |
|    101 |  131 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     99 |  132 | `		*pOut = (ph7_int64)u;` |
|      - |  133 | `	}` |
|    103 |  134 | `	return 1;` |
|     82 |  135 | `}` |
|      - |  136 | `/* Is byte c one of the nSep thousand separators in zSep? */` |
|     52 |  137 | `static int FvIsThousandSep(const char *zSep,int nSep,int c){` |
|      - |  138 | `	int i;` |
|    102 |  139 | `	for( i=0; i<nSep; i++ ){` |
|     96 |  140 | `		if( (unsigned char)zSep[i] == (unsigned char)c ){ return 1; }` |
|     27 |  141 | `	}` |
|      8 |  142 | `	return 0;` |
|     27 |  143 | `}` |
|      - |  144 | `/*` |
|      - |  145 | ` * FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure.` |
|      - |  146 | ` *` |
|      - |  147 | ` * decSep is the byte that separates the fractional part (php's "decimal" option,` |
|      - |  148 | ` * '.' by default) and zSep[0..nSep) the set that may group the INTEGER part when` |
|      - |  149 | ` * FILTER_FLAG_ALLOW_THOUSAND is set (php's "thousand" option, "',." by default).` |
|      - |  150 | ` * The two sets overlap by default, and php tests the decimal separator FIRST —` |
|      - |  151 | `` * which is why `decimal => ','` leaves '.' working as a group separator.`` |
|      - |  152 | ` *` |
|      - |  153 | ` * The number is normalized into zBuf as a plain C double literal (separators` |
|      - |  154 | ` * dropped, decSep rewritten to '.') and handed to strtod.` |
|      - |  155 | ` */` |
|    128 |  156 | `static int FvValidateFloat(const char *z,int n,int flags,int decSep,` |
|      2 |  157 | `                           const char *zSep,int nSep,double *pOut){` |
|      - |  158 | `	/* decSep is a BYTE value (0..255): a separator above 127 — php takes any` |
|      - |  159 | `	 * single byte, including one out of a UTF-8 sequence — must not be compared` |
|      - |  160 | `	 * against a sign-extended char. */` |
|      - |  161 | `	char zBuf[512];` |
|    130 |  162 | `	int i = 0, m = 0, seenDigit = 0, grouped = 0, nGroup = 0, runLen;` |
|    130 |  163 | `	int hasExp = 0, expNonZero = 0, hasDot = 0;` |
|    130 |  164 | `	double d = 0;` |
|    130 |  165 | `	FvTrim(&z,&n);` |
|      - |  166 | `	/* Bound the input: zBuf[512] holds the separator-stripped copy, and the cap` |
|      - |  167 | `	 * also rejects the pathological 500+ digit floats PHP refuses. */` |
|    130 |  168 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|    130 |  169 | `	if( i<n && (z[i]=='+'\|\|z[i]=='-') ){ zBuf[m++] = z[i]; i++; }` |
|      - |  170 | `	/* The integer part: digit runs, optionally separated by a thousand separator.` |
|      - |  171 | `	 * A separator anywhere means the runs must GROUP — a leading run of 1..3` |
|      - |  172 | `	 * digits then runs of exactly 3 ("1,000" and "1'234,567" ok, "1,5" and` |
|      - |  173 | `	 * "1234,567" rejected); with no separator the run is any length at all` |
|      - |  174 | `	 * (including zero, which is how ".5" parses). */` |
|     64 |  175 | `	for(;;){` |
|    168 |  176 | `		runLen = 0;` |
|    410 |  177 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){ zBuf[m++] = z[i]; i++; runLen++; }` |
|    168 |  178 | `		if( runLen>0 ){ seenDigit = 1; }` |
|    166 |  179 | `		if( i<n && (unsigned char)z[i]!=decSep && (flags & FV_FLAG_ALLOW_THOUSAND)` |
|     70 |  180 | `		 && FvIsThousandSep(zSep,nSep,z[i]) ){` |
|     46 |  181 | `			if( nGroup==0 ){ if( runLen<1 \|\| runLen>3 ){ return 0; } }` |
|      8 |  182 | `			else if( runLen!=3 ){ return 0; }` |
|     40 |  183 | `			grouped = 1; nGroup++;` |
|     40 |  184 | `			i++;                       /* drop the separator itself */` |
|     40 |  185 | `			continue;` |
|      - |  186 | `		}` |
|    124 |  187 | `		if( grouped && runLen!=3 ){ return 0; } /* the run that closes a grouped number */` |
|    116 |  188 | `		break;` |
|    ! 0 |  189 | `	}` |
|    116 |  190 | `	if( i<n && (unsigned char)z[i]==decSep ){` |
|     52 |  191 | `		zBuf[m++] = '.';` |
|     52 |  192 | `		hasDot = 1;` |
|     52 |  193 | `		i++;` |
|     98 |  194 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){ zBuf[m++] = z[i]; i++; seenDigit = 1; }` |
|     25 |  195 | `	}` |
|    116 |  196 | `	if( !seenDigit ){ return 0; }` |
|    112 |  197 | `	if( i<n && (z[i]=='e'\|\|z[i]=='E') ){` |
|     38 |  198 | `		zBuf[m++] = z[i];` |
|     38 |  199 | `		i++;` |
|     38 |  200 | `		if( i<n && (z[i]=='+'\|\|z[i]=='-') ){ zBuf[m++] = z[i]; i++; }` |
|     38 |  201 | `		if( i>=n \|\| !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    122 |  202 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     86 |  203 | `			if( z[i]!='0' ){ expNonZero = 1; }` |
|     86 |  204 | `			zBuf[m++] = z[i]; i++;` |
|      2 |  205 | `		}` |
|     38 |  206 | `		hasExp = 1;` |
|     18 |  207 | `	}` |
|    112 |  208 | `	if( i!=n ){ return 0; } /* trailing junk */` |
|      - |  209 | `	/* The grammar above guarantees zBuf[0..m) is a clean ASCII decimal float (no hex /` |
|      - |  210 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - |  211 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - |  212 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - |  213 | `	 * correctly rounded. Every byte written to zBuf consumed one input byte, so` |
|      - |  214 | `	 * m <= n <= 500 < sizeof(zBuf) and the NUL below is in range.` |
|      - |  215 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - |  216 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|    100 |  217 | `	zBuf[m] = 0;` |
|    100 |  218 | `	errno = 0;` |
|    100 |  219 | `	d = strtod(zBuf,0);` |
|    100 |  220 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 |  221 | `		return 0;` |
|      - |  222 | `	}` |
|      - |  223 | `	/* php's own strtod reports a zero answer carrying a non-zero EXPONENT as an` |
|      - |  224 | `	 * underflow, where glibc's leaves errno alone: "0e1", "0.e5" and ".0e5" are` |
|      - |  225 | `	 * refused while "0", "0.0" and "0e0" are the float zero. */` |
|     86 |  226 | `	if( d == 0.0 && hasExp && expNonZero ){ return 0; }` |
|      - |  227 | `	/* An INTEGER-shaped literal is answered through php's long path, which has no` |
|      - |  228 | `	 * signed zero: "-0" is the float +0.0 where "-0.0" and "-0e0" stay negative. */` |
|     82 |  229 | `	if( d == 0.0 && !hasDot && !hasExp ){ d = 0.0; }` |
|     82 |  230 | `	*pOut = d;` |
|     82 |  231 | `	return 1;` |
|     66 |  232 | `}` |
|      - |  233 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - |  234 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - |  235 | ` * false, NOT failures. */` |
|     43 |  236 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     43 |  237 | `	FvTrim(&z,&n);` |
|     40 |  238 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     35 |  239 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 |  240 | `		*pBool = 1; return 1;` |
|      - |  241 | `	}` |
|     30 |  242 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     17 |  243 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     17 |  244 | `		*pBool = 0; return 1;` |
|      - |  245 | `	}` |
|     12 |  246 | `	return 0;` |
|     20 |  247 | `}` |
|      - |  248 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. The four octets` |
|      - |  249 | ` * land in aOut[] (which the range flags below read); pass 0 to only validate. */` |
|    593 |  250 | `static int FvValidateIp4(const char *z,int n,unsigned char *aOut){` |
|    593 |  251 | `	int i = 0, parts = 0;` |
|   2013 |  252 | `	while( i<n ){` |
|   1657 |  253 | `		int val = 0, digits = 0, start = i;` |
|   4523 |  254 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|   2944 |  255 | `			val = val*10 + (z[i]-'0');` |
|   2944 |  256 | `			if( val>255 ){ return 0; }` |
|   2868 |  257 | `			digits++; i++;` |
|      2 |  258 | `		}` |
|   1581 |  259 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|   1456 |  260 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|   1452 |  261 | `		if( parts>3 ){ return 0; }` |
|   1452 |  262 | `		if( aOut ){ aOut[parts] = (unsigned char)val; }` |
|   1452 |  263 | `		parts++;` |
|   1452 |  264 | `		if( i<n ){` |
|   1096 |  265 | `			if( z[i]!='.' ){ return 0; }` |
|   1066 |  266 | `			i++;` |
|   1066 |  267 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|    532 |  268 | `		}` |
|      2 |  269 | `	}` |
|    358 |  270 | `	return parts==4;` |
|    298 |  271 | `}` |
|      - |  272 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - |  273 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1, and` |
|      - |  274 | ` * writes the bytes it read at aOut (which must have room for 16). */` |
|    394 |  275 | `static int FvIp6Hextets(const char *z,int n,unsigned char *aOut){` |
|    394 |  276 | `	int i = 0, segStart = 0, groups = 0;` |
|    394 |  277 | `	if( n==0 ){ return 0; }` |
|   2268 |  278 | `	while( i<=n ){` |
|   1886 |  279 | `		if( i==n \|\| z[i]==':' ){` |
|    508 |  280 | `			int segLen = i - segStart, j, isV4 = 0;` |
|    508 |  281 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|   1698 |  282 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|    508 |  283 | `			if( isV4 ){` |
|     34 |  284 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     34 |  285 | `				if( groups>6 ){ return -1; }` |
|     34 |  286 | `				if( !FvValidateIp4(z+segStart,segLen,aOut ? &aOut[groups*2] : 0) ){ return -1; }` |
|     28 |  287 | `				groups += 2;` |
|     15 |  288 | `			}else{` |
|    476 |  289 | `				int val = 0;` |
|    476 |  290 | `				if( segLen>4 ){ return -1; }` |
|    476 |  291 | `				if( groups>7 ){ return -1; }` |
|   1598 |  292 | `				for( j=segStart; j<i; j++ ){` |
|   1128 |  293 | `					int h = SyHexToint((unsigned char)z[j]);` |
|   1128 |  294 | `					if( h<0 ){ return -1; }` |
|   1125 |  295 | `					val = val*16 + h;` |
|    564 |  296 | `				}` |
|    473 |  297 | `				if( aOut ){` |
|    473 |  298 | `					aOut[groups*2]   = (unsigned char)(val>>8);` |
|    473 |  299 | `					aOut[groups*2+1] = (unsigned char)(val & 0xFF);` |
|    235 |  300 | `				}` |
|    473 |  301 | `				groups++;` |
|      - |  302 | `			}` |
|    499 |  303 | `			segStart = i+1;` |
|    248 |  304 | `		}` |
|   1878 |  305 | `		i++;` |
|      4 |  306 | `	}` |
|    385 |  307 | `	return groups;` |
|    199 |  308 | `}` |
|      - |  309 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present.` |
|      - |  310 | ` * The 16 address bytes land in aOut[] when it is not NULL. */` |
|    244 |  311 | `static int FvValidateIp6(const char *z,int n,unsigned char *aOut){` |
|    244 |  312 | `	const char *zDbl = 0;` |
|      - |  313 | `	int i, ga, gb;` |
|      - |  314 | `	unsigned char aHead[16], aTail[16];` |
|   1956 |  315 | `	for( i=0; i+1<n; i++ ){` |
|   1718 |  316 | `		if( z[i]==':' && z[i+1]==':' ){` |
|    233 |  317 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|    231 |  318 | `			zDbl = z+i;` |
|    114 |  319 | `		}` |
|    860 |  320 | `	}` |
|    242 |  321 | `	SyZero(aHead,(sxu32)sizeof(aHead));` |
|    242 |  322 | `	SyZero(aTail,(sxu32)sizeof(aTail));` |
|    242 |  323 | `	if( zDbl==0 ){` |
|     15 |  324 | `		if( FvIp6Hextets(z,n,aHead)!=8 ){ return 0; }` |
|    ! 0 |  325 | `		if( aOut ){ SyMemcpy(aHead,aOut,16); }` |
|    ! 0 |  326 | `		return 1;` |
|    ! 0 |  327 | `	}else{` |
|    229 |  328 | `		int lenA = (int)(zDbl - z);` |
|    229 |  329 | `		int lenB = n - lenA - 2;` |
|    229 |  330 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA,aHead);` |
|    229 |  331 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB,aTail);` |
|    229 |  332 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|    229 |  333 | `		if( (ga+gb)>7 ){ return 0; } /* "::" stands for at least one zero group */` |
|    229 |  334 | `		if( aOut ){` |
|      - |  335 | `			/* the head groups, then the elided zeros, then the tail groups */` |
|    220 |  336 | `			SyZero(aOut,16);` |
|    220 |  337 | `			if( ga>0 ){ SyMemcpy(aHead,aOut,(sxu32)(ga*2)); }` |
|    220 |  338 | `			if( gb>0 ){ SyMemcpy(aTail,&aOut[16-gb*2],(sxu32)(gb*2)); }` |
|    109 |  339 | `		}` |
|    229 |  340 | `		return 1;` |
|      - |  341 | `	}` |
|    124 |  342 | `}` |
|      - |  343 | `/*` |
|      - |  344 | ` * php's three IP RANGE flags, which describe what an address is FOR rather than` |
|      - |  345 | ` * how it is spelled. Every boundary below was derived by sweeping php 8.5 across` |
|      - |  346 | ` * the whole IPv4 space and across the IPv6 first/second hextet space.` |
|      - |  347 | ` *` |
|      - |  348 | ` *   NO_PRIV_RANGE   RFC1918: 10/8, 172.16/12, 192.168/16 -- and fc00::/7.` |
|      - |  349 | ` *   NO_RES_RANGE    0/8, 127/8, 169.254/16, 240/4 -- and ::, ::1,` |
|      - |  350 | ` *                   ::ffff:0:0/96, ::ffff:0:0:0/96, fe80::/10.` |
|      - |  351 | ` *   GLOBAL_RANGE    "not globally reachable": both sets above, plus the` |
|      - |  352 | ` *                   shared / benchmarking / documentation blocks.` |
|      - |  353 | ` */` |
|    328 |  354 | `static int FvIpRangeOk4(const unsigned char *ip,int flags){` |
|      - |  355 | `	int priv, res;` |
|    328 |  356 | `	if( (flags & (FV_FLAG_NO_PRIV_RANGE\|FV_FLAG_NO_RES_RANGE\|FV_FLAG_GLOBAL_RANGE))==0 ){` |
|     70 |  357 | `		return 1;` |
|      - |  358 | `	}` |
|    513 |  359 | `	priv = (ip[0]==10)` |
|    254 |  360 | `	    \|\| (ip[0]==172 && ip[1]>=16 && ip[1]<=31)` |
|    383 |  361 | `	    \|\| (ip[0]==192 && ip[1]==168);` |
|    376 |  362 | `	res  = (ip[0]==0) \|\| (ip[0]==127)` |
|    246 |  363 | `	    \|\| (ip[0]==169 && ip[1]==254)` |
|    383 |  364 | `	    \|\| (ip[0]>=240);` |
|    259 |  365 | `	if( (flags & FV_FLAG_NO_PRIV_RANGE) && priv ){ return 0; }` |
|    243 |  366 | `	if( (flags & FV_FLAG_NO_RES_RANGE) && res ){ return 0; }` |
|    223 |  367 | `	if( flags & FV_FLAG_GLOBAL_RANGE ){` |
|     67 |  368 | `		if( priv \|\| res ){ return 0; }` |
|     49 |  369 | `		if( ip[0]==100 && ip[1]>=64 && ip[1]<=127 ){ return 0; }            /* 100.64/10 shared */` |
|     45 |  370 | `		if( ip[0]==192 && ip[1]==0 && (ip[2]==0 \|\| ip[2]==2) ){ return 0; } /* 192.0.0/24, TEST-NET-1 */` |
|     41 |  371 | `		if( ip[0]==198 && (ip[1]==18 \|\| ip[1]==19) ){ return 0; }           /* 198.18/15 benchmarking */` |
|     37 |  372 | `		if( ip[0]==198 && ip[1]==51 && ip[2]==100 ){ return 0; }            /* TEST-NET-2 */` |
|     35 |  373 | `		if( ip[0]==203 && ip[1]==0 && ip[2]==113 ){ return 0; }             /* TEST-NET-3 */` |
|     16 |  374 | `	}` |
|    189 |  375 | `	return 1;` |
|    165 |  376 | `}` |
|    469 |  377 | `static int FvIp6BytesZero(const unsigned char *ip,int iFrom,int iTo){` |
|      - |  378 | `	int i;` |
|   1347 |  379 | `	for( i=iFrom; i<iTo; i++ ){ if( ip[i] ){ return 0; } }` |
|     59 |  380 | `	return 1;` |
|    235 |  381 | `}` |
|    220 |  382 | `static int FvIpRangeOk6(const unsigned char *ip,int flags){` |
|      - |  383 | `	int priv, res;` |
|    220 |  384 | `	if( (flags & (FV_FLAG_NO_PRIV_RANGE\|FV_FLAG_NO_RES_RANGE\|FV_FLAG_GLOBAL_RANGE))==0 ){` |
|     52 |  385 | `		return 1;` |
|      - |  386 | `	}` |
|    169 |  387 | `	priv = (ip[0] & 0xFE) == 0xFC;                                          /* fc00::/7 */` |
|    253 |  388 | `	res  = (FvIp6BytesZero(ip,0,15) && (ip[15]==0 \|\| ip[15]==1))            /* ::, ::1 */` |
|    156 |  389 | `	    \|\| (FvIp6BytesZero(ip,0,10) && ip[10]==0xFF && ip[11]==0xFF)        /* ::ffff:0:0/96 */` |
|    144 |  390 | `	    \|\| (FvIp6BytesZero(ip,0,8) && ip[8]==0xFF && ip[9]==0xFF` |
|      8 |  391 | `	        && ip[10]==0 && ip[11]==0)                                      /* ::ffff:0:0:0/96 */` |
|    252 |  392 | `	    \|\| (ip[0]==0xFE && (ip[1] & 0xC0)==0x80);                           /* fe80::/10 */` |
|    169 |  393 | `	if( (flags & FV_FLAG_NO_PRIV_RANGE) && priv ){ return 0; }` |
|    161 |  394 | `	if( (flags & FV_FLAG_NO_RES_RANGE) && res ){ return 0; }` |
|    137 |  395 | `	if( flags & FV_FLAG_GLOBAL_RANGE ){` |
|     43 |  396 | `		if( priv \|\| res ){ return 0; }` |
|     27 |  397 | `		if( ip[0]==0x20 && ip[1]==0x01 && (ip[2] & 0xFE)==0 ){ return 0; }  /* 2001::/23 protocol assignments */` |
|     23 |  398 | `		if( ip[0]==0x20 && ip[1]==0x01 && ip[2]==0x0D && ip[3]==0xB8 ){ return 0; } /* 2001:db8::/32 documentation */` |
|     21 |  399 | `		if( ip[0]==0x20 && ip[1]==0x02 ){ return 0; }                       /* 2002::/16 6to4 */` |
|     19 |  400 | `		if( ip[0]==0x01 && ip[1]==0x00 && FvIp6BytesZero(ip,2,8) ){ return 0; } /* 100::/64 discard */` |
|      8 |  401 | `	}` |
|    111 |  402 | `	return 1;` |
|    111 |  403 | `}` |
|    565 |  404 | `static int FvValidateIp(const char *z,int n,int flags){` |
|    565 |  405 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|      - |  406 | `	unsigned char aIp[16];` |
|    565 |  407 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|    565 |  408 | `	if( v4 && FvValidateIp4(z,n,aIp) ){ return FvIpRangeOk4(aIp,flags); }` |
|    239 |  409 | `	if( v6 && FvValidateIp6(z,n,aIp) ){ return FvIpRangeOk6(aIp,flags); }` |
|     21 |  410 | `	return 0;` |
|    284 |  411 | `}` |
|      - |  412 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     37 |  413 | `static int FvValidateMac(const char *z,int n){` |
|      - |  414 | `	int i;` |
|     37 |  415 | `	if( n==17 ){` |
|      - |  416 | `		/* the two byte-per-group spellings: XX:XX:XX:XX:XX:XX and XX-XX-...-XX,` |
|      - |  417 | `		 * with the SAME separator throughout */` |
|     14 |  418 | `		char sep = z[2];` |
|     14 |  419 | `		if( sep!=':' && sep!='-' ){ return 0; }` |
|    190 |  420 | `		for( i=0; i<17; i++ ){` |
|    182 |  421 | `			if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|    128 |  422 | `			else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     90 |  423 | `		}` |
|     10 |  424 | `		return 1;` |
|      - |  425 | `	}` |
|     25 |  426 | `	if( n==14 ){` |
|      - |  427 | `		/* php's third spelling, the dotted one Cisco writes: XXXX.XXXX.XXXX */` |
|    104 |  428 | `		for( i=0; i<14; i++ ){` |
|    100 |  429 | `			if( i==4 \|\| i==9 ){ if( z[i]!='.' ){ return 0; } }` |
|     86 |  430 | `			else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     48 |  431 | `		}` |
|      5 |  432 | `		return 1;` |
|      - |  433 | `	}` |
|     15 |  434 | `	return 0;` |
|     20 |  435 | `}` |
|      - |  436 | `#ifdef PH7_ENABLE_PCRE` |
|      - |  437 | `/*` |
|      - |  438 | ` * FILTER_VALIDATE_EMAIL is a REGEX in php — one built on Michael Rushton's, cut` |
|      - |  439 | ` * down to routable addresses — and the two spellings below are php 8.5's own,` |
|      - |  440 | ` * byte for byte: the second is the one FILTER_FLAG_EMAIL_UNICODE selects, and` |
|      - |  441 | ` * differs only by \pL\pN in the local-part classes (with /u, so a non-ASCII` |
|      - |  442 | ` * local part is a letter rather than a stray byte).` |
|      - |  443 | ` *` |
|      - |  444 | ` * What the hand-rolled screen this replaces could not say: a quoted local part` |
|      - |  445 | `` * (`"a@b"@c.com`), an address literal domain (`a@[127.0.0.1]`, `a@[IPv6:::1]`),`` |
|      - |  446 | ` * the 64-byte local-part and 254-byte total limits, the rule that a bare IPv4` |
|      - |  447 | `` * domain is not an address (`a@1.2.3.4`), and the one that the last label must`` |
|      - |  448 | ` * start with a letter. It accepted a non-ASCII local part unconditionally,` |
|      - |  449 | ` * which is the EMAIL_UNICODE answer for a program that did not ask for it.` |
|      - |  450 | ` */` |
|      - |  451 | `static const char zFvEmailRe[] =` |
|      - |  452 | `	"/^(?!(?:(?:\\x22?\\x5C[\\x00-\\x7E]\\x22?)\|(?:\\x22?[^\\x5C\\x22]\\x22?)){255,})(?!(?:(?:\\x22?\\x5C"` |
|      - |  453 | `	"[\\x00-\\x7E]\\x22?)\|(?:\\x22?[^\\x5C\\x22]\\x22?)){65,}@)(?:(?:[\\x21\\x23-\\x27\\x2A\\x2B\\x2D\\x2F"` |
|      - |  454 | `	"-\\x39\\x3D\\x3F\\x5E-\\x7E]+)\|(?:\\x22(?:[\\x01-\\x08\\x0B\\x0C\\x0E-\\x1F\\x21\\x23-\\x5B\\x5D-\\x"` |
|      - |  455 | `	"7F]\|(?:\\x5C[\\x00-\\x7F]))*\\x22))(?:\\.(?:(?:[\\x21\\x23-\\x27\\x2A\\x2B\\x2D\\x2F-\\x39\\x3D\\x3F"` |
|      - |  456 | `	"\\x5E-\\x7E]+)\|(?:\\x22(?:[\\x01-\\x08\\x0B\\x0C\\x0E-\\x1F\\x21\\x23-\\x5B\\x5D-\\x7F]\|(?:\\x5C[\\x"` |
|      - |  457 | `	"00-\\x7F]))*\\x22)))*@(?:(?:(?!.*[^.]{64,})(?:(?:(?:xn--)?[a-z0-9]+(?:-+[a-z0-9]+)*\\.){1,126}){1,}("` |
|      - |  458 | `	"?:(?:[a-z][a-z0-9]*)\|(?:(?:xn--)[a-z0-9]+))(?:-+[a-z0-9]+)*)\|(?:\\[(?:(?:IPv6:(?:(?:[a-f0-9]{1,4}(?:"` |
|      - |  459 | `	":[a-f0-9]{1,4}){7})\|(?:(?!(?:.*[a-f0-9][:\\]]){7,})(?:[a-f0-9]{1,4}(?::[a-f0-9]{1,4}){0,5})?::(?:[a-"` |
|      - |  460 | `	"f0-9]{1,4}(?::[a-f0-9]{1,4}){0,5})?)))\|(?:(?:IPv6:(?:(?:[a-f0-9]{1,4}(?::[a-f0-9]{1,4}){5}:)\|(?:(?!("` |
|      - |  461 | `	"?:.*[a-f0-9]:){5,})(?:[a-f0-9]{1,4}(?::[a-f0-9]{1,4}){0,3})?::(?:[a-f0-9]{1,4}(?::[a-f0-9]{1,4}){0,3"` |
|      - |  462 | `	"}:)?)))?(?:(?:25[0-5])\|(?:2[0-4][0-9])\|(?:1[0-9]{2})\|(?:[1-9]?[0-9]))(?:\\.(?:(?:25[0-5])\|(?:2[0-4]["` |
|      - |  463 | `	"0-9])\|(?:1[0-9]{2})\|(?:[1-9]?[0-9]))){3}))\\]))$/iD";` |
|      - |  464 | `static const char zFvEmailReUni[] =` |
|      - |  465 | `	"/^(?!(?:(?:\\x22?\\x5C[\\x00-\\x7E]\\x22?)\|(?:\\x22?[^\\x5C\\x22]\\x22?)){255,})(?!(?:(?:\\x22?\\x5C"` |
|      - |  466 | `	"[\\x00-\\x7E]\\x22?)\|(?:\\x22?[^\\x5C\\x22]\\x22?)){65,}@)(?:(?:[\\x21\\x23-\\x27\\x2A\\x2B\\x2D\\x2F"` |
|      - |  467 | `	"-\\x39\\x3D\\x3F\\x5E-\\x7E\\pL\\pN]+)\|(?:\\x22(?:[\\x01-\\x08\\x0B\\x0C\\x0E-\\x1F\\x21\\x23-\\x5B\\x"` |
|      - |  468 | `	"5D-\\x7F\\pL\\pN]\|(?:\\x5C[\\x00-\\x7F]))*\\x22))(?:\\.(?:(?:[\\x21\\x23-\\x27\\x2A\\x2B\\x2D\\x2F-\\x"` |
|      - |  469 | `	"39\\x3D\\x3F\\x5E-\\x7E\\pL\\pN]+)\|(?:\\x22(?:[\\x01-\\x08\\x0B\\x0C\\x0E-\\x1F\\x21\\x23-\\x5B\\x5D"` |
|      - |  470 | `	"-\\x7F\\pL\\pN]\|(?:\\x5C[\\x00-\\x7F]))*\\x22)))*@(?:(?:(?!.*[^.]{64,})(?:(?:(?:xn--)?[a-z0-9]+(?:-+"` |
|      - |  471 | `	"[a-z0-9]+)*\\.){1,126}){1,}(?:(?:[a-z][a-z0-9]*)\|(?:(?:xn--)[a-z0-9]+))(?:-+[a-z0-9]+)*)\|(?:\\[(?:(?"` |
|      - |  472 | `	":IPv6:(?:(?:[a-f0-9]{1,4}(?::[a-f0-9]{1,4}){7})\|(?:(?!(?:.*[a-f0-9][:\\]]){7,})(?:[a-f0-9]{1,4}(?::["` |
|      - |  473 | `	"a-f0-9]{1,4}){0,5})?::(?:[a-f0-9]{1,4}(?::[a-f0-9]{1,4}){0,5})?)))\|(?:(?:IPv6:(?:(?:[a-f0-9]{1,4}(?:"` |
|      - |  474 | `	":[a-f0-9]{1,4}){5}:)\|(?:(?!(?:.*[a-f0-9]:){5,})(?:[a-f0-9]{1,4}(?::[a-f0-9]{1,4}){0,3})?::(?:[a-f0-9"` |
|      - |  475 | `	"]{1,4}(?::[a-f0-9]{1,4}){0,3}:)?)))?(?:(?:25[0-5])\|(?:2[0-4][0-9])\|(?:1[0-9]{2})\|(?:[1-9]?[0-9]))(?:"` |
|      - |  476 | `	"\\.(?:(?:25[0-5])\|(?:2[0-4][0-9])\|(?:1[0-9]{2})\|(?:[1-9]?[0-9]))){3}))\\]))$/iDu";` |
|      - |  477 | `#endif /* PH7_ENABLE_PCRE */` |
|      - |  478 | `/*` |
|      - |  479 | ` * FILTER_VALIDATE_EMAIL. The regex above is the whole rule wherever PCRE is` |
|      - |  480 | ` * built in; a PCRE-less build (the tiny target) keeps the approximation this` |
|      - |  481 | ` * engine has always had, which is the same trade the REGEXP filter makes.` |
|      - |  482 | ` */` |
|    204 |  483 | `static int FvValidateEmail(ph7_context *pCtx,const char *z,int n,int flags){` |
|      - |  484 | `	/* The maximum length of an e-mail address is 320 octets, per RFC 2821. */` |
|    204 |  485 | `	if( n>320 ){ return 0; }` |
|      - |  486 | `#ifdef PH7_ENABLE_PCRE` |
|      - |  487 | `	{` |
|    204 |  488 | `		const char *zRe = (flags & FV_FLAG_EMAIL_UNICODE) ? zFvEmailReUni : zFvEmailRe;` |
|    204 |  489 | `		int matched = 0;` |
|    204 |  490 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,(int)SyStrlen(zRe),z,n,&matched)!=SXRET_OK ){` |
|    ! 0 |  491 | `			return 0;` |
|      - |  492 | `		}` |
|    204 |  493 | `		return matched;` |
|      - |  494 | `	}` |
|      - |  495 | `#else` |
|      - |  496 | `	{` |
|      - |  497 | `		int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - |  498 | `		const char *zDom;` |
|      - |  499 | `		SXUNUSED(pCtx); SXUNUSED(flags);` |
|      - |  500 | `		if( n==0 ){ return 0; }` |
|      - |  501 | `		for( i=0; i<n; i++ ){` |
|      - |  502 | `			if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|      - |  503 | `		}` |
|      - |  504 | `		if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|      - |  505 | `		localLen = at;` |
|      - |  506 | `		zDom = z + at + 1;` |
|      - |  507 | `		domLen = n - at - 1;` |
|      - |  508 | `		if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|      - |  509 | `		for( i=0; i<localLen; i++ ){` |
|      - |  510 | `			unsigned char c = (unsigned char)z[i];` |
|      - |  511 | `			if( c<=' ' ){ return 0; }` |
|      - |  512 | `			if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|      - |  513 | `		}` |
|      - |  514 | `		if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|      - |  515 | `		labelStart = 0;` |
|      - |  516 | `		for( i=0; i<=domLen; i++ ){` |
|      - |  517 | `			if( i==domLen \|\| zDom[i]=='.' ){` |
|      - |  518 | `				int ll = i - labelStart;` |
|      - |  519 | `				if( ll==0 ){ return 0; } /* consecutive dots */` |
|      - |  520 | `				if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|      - |  521 | `				if( i<domLen ){ dotCount++; }` |
|      - |  522 | `				labelStart = i+1;` |
|      - |  523 | `			}else{` |
|      - |  524 | `				unsigned char c = (unsigned char)zDom[i];` |
|      - |  525 | `				if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - |  526 | `			}` |
|      - |  527 | `		}` |
|      - |  528 | `		if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      - |  529 | `		return 1;` |
|      - |  530 | `	}` |
|      - |  531 | `#endif /* PH7_ENABLE_PCRE */` |
|    104 |  532 | `}` |
|   1632 |  533 | `static int FvIsAlnum(unsigned char c){` |
|   1632 |  534 | `	return (c>='a'&&c<='z') \|\| (c>='A'&&c<='Z') \|\| (c>='0'&&c<='9');` |
|      4 |  535 | `}` |
|      - |  536 | `/*` |
|      - |  537 | ` * FILTER_VALIDATE_DOMAIN, a byte-for-byte port of php's own walk.` |
|      - |  538 | ` *` |
|      - |  539 | ` * Without FILTER_FLAG_HOSTNAME php checks LENGTHS and nothing else — one` |
|      - |  540 | ` * trailing dot is ignored, the rest is at most 253 bytes and splits into labels` |
|      - |  541 | ``  * of at most 63 with no empty one — so `" spaced "`, `"a b"`, `"ex@mple.com"` `` |
|      - |  542 | ` * and the empty string are all domains there. The flag adds the host-NAME rule:` |
|      - |  543 | ` * a label carries only alphanumerics and hyphens and does not start or end with` |
|      - |  544 | ` * one of the hyphens.` |
|      - |  545 | ` *` |
|      - |  546 | ` * Two edges follow from php walking the ORIGINAL string rather than a trimmed` |
|      - |  547 | ` * copy: a lone "." fails on the first-character test (the trailing-dot rule` |
|      - |  548 | ` * moved the END, not the start), and a hyphen is refused only when the byte` |
|      - |  549 | `` * after it is the string's NUL — so `"0-"` is not a hostname and `"0-."` is.`` |
|      - |  550 | ` */` |
|    265 |  551 | `static int FvValidateDomain(const char *z,int n,int flags){` |
|    265 |  552 | `	int hostname = (flags & FV_FLAG_HOSTNAME) != 0;` |
|    265 |  553 | `	int i, iEnd = n, nLen = n, nLabel = 1;` |
|    265 |  554 | `	if( n>0 && z[n-1]=='.' ){ iEnd = n-1; nLen = n-1; } /* the final dot is not part of the name */` |
|    265 |  555 | `	if( nLen>253 ){ return 0; }` |
|      - |  556 | `	/* php reads the first byte even for the empty string, where it is the NUL` |
|      - |  557 | `	 * terminator: "" is a domain, and is not a hostname. */` |
|    263 |  558 | `	if( n>0 && z[0]=='.' ){ return 0; }` |
|    249 |  559 | `	if( hostname && !(n>0 && FvIsAlnum((unsigned char)z[0])) ){ return 0; }` |
|   3349 |  560 | `	for( i=0; i<iEnd; i++ ){` |
|   3161 |  561 | `		unsigned char c = (unsigned char)z[i];` |
|   3161 |  562 | `		unsigned char nx = (i+1<n) ? (unsigned char)z[i+1] : 0; /* php's NUL byte */` |
|   3161 |  563 | `		if( c=='.' ){` |
|      - |  564 | `			/* i>0 here: a leading dot was refused above */` |
|    691 |  565 | `			if( nx=='.' \|\| (hostname && (!FvIsAlnum((unsigned char)z[i-1]) \|\| !FvIsAlnum(nx))) ){` |
|     14 |  566 | `				return 0;` |
|      - |  567 | `			}` |
|    679 |  568 | `			nLabel = 1;` |
|    341 |  569 | `		}else{` |
|   2473 |  570 | `			if( nLabel>63 \|\| (hostname && (c!='-' \|\| nx==0) && !FvIsAlnum(c)) ){ return 0; }` |
|   2447 |  571 | `			nLabel++;` |
|      - |  572 | `		}` |
|   1563 |  573 | `	}` |
|    191 |  574 | `	return 1;` |
|    134 |  575 | `}` |
|      - |  576 | `/* The bytes php's SANITIZE_URL map keeps: every printable ASCII byte. Shared` |
|      - |  577 | ` * with the URL VALIDATION below, which php runs that map over first. */` |
|   4351 |  578 | `static int FvUrlAllowed(unsigned char c){` |
|   4351 |  579 | `	return c>=33 && c<=126;` |
|      3 |  580 | `}` |
|      - |  581 | `/*` |
|      - |  582 | ` * php's userinfo rule: the bytes it lets stand in a URL's user and password.` |
|      - |  583 | `` * A percent escape is `%` + a DIGIT + a hex digit, which is php's own asymmetry`` |
|      - |  584 | ` * and not a transcription slip.` |
|      - |  585 | ` */` |
|     30 |  586 | `static int FvUserinfoValid(const char *z,int n){` |
|     30 |  587 | `	int i = 0;` |
|     62 |  588 | `	while( i<n ){` |
|     34 |  589 | `		unsigned char c = (unsigned char)z[i];` |
|     32 |  590 | `		if( FvIsAlnum(c) \|\| c=='-' \|\| c=='.' \|\| c=='_' \|\| c=='~' \|\| c=='!' \|\| c=='$'` |
|    ! 0 |  591 | `		 \|\| c=='&' \|\| c=='\'' \|\| c=='(' \|\| c==')' \|\| c=='*' \|\| c=='+' \|\| c==','` |
|      2 |  592 | `		 \|\| c==';' \|\| c=='=' \|\| c==':' ){` |
|     34 |  593 | `			i++;` |
|     18 |  594 | `		}else if( c=='%' && i+2<n && SyisDigit((unsigned char)z[i+1])` |
|    ! 0 |  595 | `		       && SyHexToint((unsigned char)z[i+2])>=0 ){` |
|    ! 0 |  596 | `			i += 3;` |
|    ! 0 |  597 | `		}else{` |
|    ! 0 |  598 | `			return 0;` |
|      - |  599 | `		}` |
|      2 |  600 | `	}` |
|     30 |  601 | `	return 1;` |
|     16 |  602 | `}` |
|    334 |  603 | `static int FvSyStrEqNoCase(const SyString *pStr,const char *zLit){` |
|    334 |  604 | `	sxu32 n = SyStrlen(zLit);` |
|    334 |  605 | `	return pStr->nByte==n && SyStrnicmp(pStr->zString,zLit,n)==0;` |
|      2 |  606 | `}` |
|    145 |  607 | `static int FvSyStrEq(const SyString *pStr,const char *zLit){` |
|    145 |  608 | `	sxu32 n = SyStrlen(zLit);` |
|    145 |  609 | `	return pStr->nByte==n && SyMemcmp(pStr->zString,zLit,n)==0;` |
|      1 |  610 | `}` |
|      - |  611 | `/*` |
|      - |  612 | ` * FILTER_VALIDATE_URL, php's own sequence.` |
|      - |  613 | ` *` |
|      - |  614 | ` * php runs the SANITIZE_URL map over the value FIRST and fails the validation` |
|      - |  615 | ` * when a byte was dropped -- which is what refuses a space, a newline or a` |
|      - |  616 | ` * non-ASCII byte anywhere in the name. Then parse_url must succeed and answer a` |
|      - |  617 | ` * SCHEME; a host is required for every scheme except the three php names` |
|      - |  618 | `` * (`mailto:`, `news:`, `file:`), and for http/https the host must be either a`` |
|      - |  619 | ` * bracketed IPv6 literal or a name that passes the HOSTNAME domain rule -- so` |
|      - |  620 | `` * `http://x_y.com/` is not a URL while `x-y://x_y.com/` is. PATH_REQUIRED and`` |
|      - |  621 | ` * QUERY_REQUIRED test the presence of those two components, and a user or` |
|      - |  622 | ` * password that is present has to be spellable.` |
|      - |  623 | ` */` |
|    323 |  624 | `static int FvValidateUrl(const char *z,int n,int flags){` |
|      - |  625 | `	VmUrlParts sUrl;` |
|      - |  626 | `	int i;` |
|    323 |  627 | `	if( n==0 ){ return 0; }` |
|   4617 |  628 | `	for( i=0; i<n; i++ ){` |
|   4329 |  629 | `		if( !FvUrlAllowed((unsigned char)z[i]) ){ return 0; }` |
|   2150 |  630 | `	}` |
|    291 |  631 | `	SyZero(&sUrl,(sxu32)sizeof(sUrl));` |
|    291 |  632 | `	if( !PH7_VmUrlSplit(z,n,&sUrl) ){ return 0; }` |
|    257 |  633 | `	if( !sUrl.bScheme ){ return 0; }` |
|    226 |  634 | `	if( FvSyStrEqNoCase(&sUrl.sScheme,"http") \|\| FvSyStrEqNoCase(&sUrl.sScheme,"https") ){` |
|      - |  635 | `		int bIp6;` |
|    128 |  636 | `		if( !sUrl.bHost ){ return 0; }` |
|    128 |  637 | `		bIp6 = sUrl.sHost.nByte>2 && sUrl.sHost.zString[0]=='['` |
|     63 |  638 | `		    && sUrl.sHost.zString[sUrl.sHost.nByte-1]==']'` |
|    185 |  639 | `		    && FvValidateIp6(sUrl.sHost.zString+1,(int)sUrl.sHost.nByte-2,0);` |
|    128 |  640 | `		if( !bIp6 && !FvValidateDomain(sUrl.sHost.zString,(int)sUrl.sHost.nByte,FV_FLAG_HOSTNAME) ){` |
|     17 |  641 | `			return 0;` |
|      - |  642 | `		}` |
|     55 |  643 | `	}` |
|    208 |  644 | `	if( !sUrl.bHost && !FvSyStrEq(&sUrl.sScheme,"mailto")` |
|     54 |  645 | `	 && !FvSyStrEq(&sUrl.sScheme,"news") && !FvSyStrEq(&sUrl.sScheme,"file") ){` |
|     33 |  646 | `		return 0;` |
|      - |  647 | `	}` |
|    178 |  648 | `	if( (flags & FV_FLAG_PATH_REQUIRED) && !sUrl.bPath ){ return 0; }` |
|    154 |  649 | `	if( (flags & FV_FLAG_QUERY_REQUIRED) && !sUrl.bQuery ){ return 0; }` |
|     96 |  650 | `	if( sUrl.bUser && !FvUserinfoValid(sUrl.sUser.zString,(int)sUrl.sUser.nByte) ){ return 0; }` |
|     96 |  651 | `	if( sUrl.bPass && !FvUserinfoValid(sUrl.sPass.zString,(int)sUrl.sPass.nByte) ){ return 0; }` |
|     96 |  652 | `	return 1;` |
|    163 |  653 | `}` |
|      - |  654 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - |  655 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - |  656 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - |  657 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     42 |  658 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     42 |  659 | `	int i, runStart = 0;` |
|     42 |  660 | `	ph7_result_string(pCtx,"",0);` |
|    104 |  661 | `	for( i=0; i<n; i++ ){` |
|     96 |  662 | `		char c = z[i];` |
|     96 |  663 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     96 |  664 | `		if( !keep && isFloat ){` |
|     38 |  665 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 |  666 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 |  667 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 |  668 | `		}` |
|     64 |  669 | `		if( !keep ){` |
|     36 |  670 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     36 |  671 | `			runStart = i+1;` |
|     17 |  672 | `		}` |
|     33 |  673 | `	}` |
|     10 |  674 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     10 |  675 | `}` |
|      - |  676 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - |  677 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - |  678 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - |  679 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    637 |  680 | `static int FvStripByte(unsigned char c,int flags){` |
|    637 |  681 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    627 |  682 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    611 |  683 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    605 |  684 | `	return 0;` |
|    320 |  685 | `}` |
|      - |  686 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - |  687 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - |  688 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - |  689 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - |  690 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - |  691 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - |  692 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 |  693 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 |  694 | `	int i, runStart = 0;` |
|     25 |  695 | `	ph7_result_string(pCtx,"",0);` |
|    193 |  696 | `	for( i=0; i<n; i++ ){` |
|    179 |  697 | `		unsigned char c = (unsigned char)z[i];` |
|    179 |  698 | `		if( FvStripByte(c,flags) ){` |
|     13 |  699 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 |  700 | `			runStart = i+1;` |
|     13 |  701 | `			continue;` |
|      - |  702 | `		}` |
|    167 |  703 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 |  704 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 |  705 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 |  706 | `			runStart = i+1;` |
|    166 |  707 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 |  708 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 |  709 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 |  710 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 |  711 | `			runStart = i+1;` |
|      4 |  712 | `		}` |
|     79 |  713 | `	}` |
|     15 |  714 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 |  715 | `}` |
|      - |  716 | `/*` |
|      - |  717 | ` * The strip-then-encode pass php runs before SANITIZE_STRING's strip_tags, into` |
|      - |  718 | ` * a blob rather than the call context because there is a second transform after` |
|      - |  719 | ` * it. bQuotes carries the NO_ENCODE_QUOTES decision; the rest is the same` |
|      - |  720 | ` * strip/encode precedence FvSanitizeString() applies.` |
|      - |  721 | ` */` |
|     26 |  722 | `static void FvStripEncodeBlob(SyBlob *pOut,const char *z,int n,int flags,int bQuotes){` |
|      - |  723 | `	int i;` |
|    322 |  724 | `	for( i=0; i<n; i++ ){` |
|    298 |  725 | `		unsigned char c = (unsigned char)z[i];` |
|    298 |  726 | `		if( FvStripByte(c,flags) ){ continue; }` |
|    290 |  727 | `		if( (bQuotes && (c=='\'' \|\| c=='"'))` |
|    268 |  728 | `		 \|\| (c=='&' && (flags & FV_FLAG_ENCODE_AMP))` |
|    264 |  729 | `		 \|\| (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    277 |  730 | `		 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|      - |  731 | `			char zBuf[16];` |
|     34 |  732 | `			int nBuf = SyBufferFormat(zBuf,sizeof(zBuf),"&#%u;",(unsigned int)c);` |
|     34 |  733 | `			SyBlobAppend(pOut,zBuf,(sxu32)nBuf);` |
|     18 |  734 | `		}else{` |
|    260 |  735 | `			SyBlobAppend(pOut,&z[i],1);` |
|      - |  736 | `		}` |
|    147 |  737 | `	}` |
|     26 |  738 | `}` |
|      - |  739 | `/*` |
|      - |  740 | ` * FILTER_SANITIZE_STRING (php's filter id 513, whose two CONSTANT names php` |
|      - |  741 | ` * deprecated in 8.1 and the scope policy therefore does not define): strip, encode the` |
|      - |  742 | ` * quotes unless NO_ENCODE_QUOTES, then strip_tags -- and answer NULL rather` |
|      - |  743 | ` * than "" for an empty result under FILTER_FLAG_EMPTY_STRING_NULL.` |
|      - |  744 | ` */` |
|     26 |  745 | `static void FvSanitizeStripString(ph7_context *pCtx,const char *z,int n,int flags){` |
|      - |  746 | `	SyBlob sEnc;` |
|     26 |  747 | `	SyBlobInit(&sEnc,&pCtx->pVm->sAllocator);` |
|     26 |  748 | `	FvStripEncodeBlob(&sEnc,z,n,flags,(flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : 1);` |
|      - |  749 | ``	/* php's filter passes allow_tag_spaces: inside SANITIZE_STRING a `<` followed`` |
|      - |  750 | `	 * by whitespace DOES open a tag, where strip_tags() itself leaves it as text. */` |
|     26 |  751 | `	PH7_StripTagsFromString(pCtx,(const char *)SyBlobData(&sEnc),(int)SyBlobLength(&sEnc),0,0,1);` |
|     26 |  752 | `	SyBlobRelease(&sEnc);` |
|     26 |  753 | `	if( ph7_context_result_buf_length(pCtx)==0 ){` |
|      9 |  754 | `		if( flags & FV_FLAG_EMPTY_STRING_NULL ){ ph7_result_null(pCtx); }` |
|      5 |  755 | `		else{ ph7_result_string(pCtx,"",0); }` |
|      4 |  756 | `	}` |
|     26 |  757 | `}` |
|      - |  758 | `/*` |
|      - |  759 | ` * FILTER_SANITIZE_ENCODED: strip, then percent-encode every byte outside` |
|      - |  760 | ` * [A-Za-z0-9-._] with php's UPPERCASE hex.` |
|      - |  761 | ` */` |
|      9 |  762 | `static void FvSanitizeEncoded(ph7_context *pCtx,const char *z,int n,int flags){` |
|      - |  763 | `	static const char zHex[] = "0123456789ABCDEF";` |
|      - |  764 | `	int i;` |
|      9 |  765 | `	ph7_result_string(pCtx,"",0);` |
|     61 |  766 | `	for( i=0; i<n; i++ ){` |
|     53 |  767 | `		unsigned char c = (unsigned char)z[i];` |
|     53 |  768 | `		if( FvStripByte(c,flags) ){ continue; }` |
|     47 |  769 | `		if( FvIsAlnum(c) \|\| c=='-' \|\| c=='.' \|\| c=='_' ){` |
|     33 |  770 | `			ph7_result_string(pCtx,&z[i],1);` |
|     17 |  771 | `		}else{` |
|      - |  772 | `			char zBuf[3];` |
|     15 |  773 | `			zBuf[0] = '%'; zBuf[1] = zHex[c>>4]; zBuf[2] = zHex[c & 15];` |
|     15 |  774 | `			ph7_result_string(pCtx,zBuf,3);` |
|      - |  775 | `		}` |
|     24 |  776 | `	}` |
|      9 |  777 | `}` |
|      - |  778 | `/*` |
|      - |  779 | ` * FILTER_SANITIZE_ADD_SLASHES: php's addslashes(), flags and all ignored.` |
|      - |  780 | ` */` |
|      7 |  781 | `static void FvSanitizeAddSlashes(ph7_context *pCtx,const char *z,int n){` |
|      7 |  782 | `	int i, runStart = 0;` |
|      7 |  783 | `	ph7_result_string(pCtx,"",0);` |
|     57 |  784 | `	for( i=0; i<n; i++ ){` |
|     51 |  785 | `		char c = z[i];` |
|     51 |  786 | `		if( c=='\'' \|\| c=='"' \|\| c=='\\' \|\| c==0 ){` |
|      9 |  787 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 |  788 | `			if( c==0 ){ ph7_result_string(pCtx,"\\0",2); }` |
|      7 |  789 | `			else{ ph7_result_string_format(pCtx,"\\%c",c); }` |
|      9 |  790 | `			runStart = i+1;` |
|      4 |  791 | `		}` |
|     26 |  792 | `	}` |
|      7 |  793 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 |  794 | `}` |
|      - |  795 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - |  796 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - |  797 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - |  798 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - |  799 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     16 |  800 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     16 |  801 | `	int i, runStart = 0;` |
|      - |  802 | `	const char *zEnt;` |
|     16 |  803 | `	ph7_result_string(pCtx,"",0);` |
|    134 |  804 | `	for( i=0; i<n; i++ ){` |
|    119 |  805 | `		unsigned char c = (unsigned char)z[i];` |
|    119 |  806 | `		if( FvStripByte(c,flags) ){` |
|      9 |  807 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 |  808 | `			runStart = i+1;` |
|      9 |  809 | `			continue;` |
|      - |  810 | `		}` |
|    111 |  811 | `		switch( c ){` |
|      3 |  812 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 |  813 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 |  814 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 |  815 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 |  816 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 |  817 | `		default:` |
|      - |  818 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - |  819 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 |  820 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 |  821 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 |  822 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 |  823 | `				runStart = i+1;` |
|      8 |  824 | `			}` |
|     93 |  825 | `			continue; /* keep in the current run */` |
|      - |  826 | `		}` |
|     19 |  827 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 |  828 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 |  829 | `		runStart = i+1;` |
|     10 |  830 | `	}` |
|     16 |  831 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     16 |  832 | `}` |
|      - |  833 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - |  834 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - |  835 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - |  836 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - |  837 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - |  838 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - |  839 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - |  840 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - |  841 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - |  842 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - |  843 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - |  844 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - |  845 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - |  846 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - |  847 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - |  848 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - |  849 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - |  850 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - |  851 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - |  852 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - |  853 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - |  854 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - |  855 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - |  856 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - |  857 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - |  858 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - |  859 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - |  860 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - |  861 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - |  862 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - |  863 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - |  864 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - |  865 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - |  866 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - |  867 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - |  868 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - |  869 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - |  870 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - |  871 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - |  872 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - |  873 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - |  874 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - |  875 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - |  876 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - |  877 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - |  878 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - |  879 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - |  880 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - |  881 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - |  882 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - |  883 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - |  884 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - |  885 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - |  886 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - |  887 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - |  888 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - |  889 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - |  890 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - |  891 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - |  892 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - |  893 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - |  894 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - |  895 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - |  896 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - |  897 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - |  898 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - |  899 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - |  900 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - |  901 | `};` |
|      - |  902 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     64 |  903 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     64 |  904 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    498 |  905 | `	while( lo <= hi ){` |
|    476 |  906 | `		int mid = (lo + hi) / 2;` |
|    476 |  907 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    476 |  908 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    436 |  909 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      2 |  910 | `	}` |
|     24 |  911 | `	return 0;` |
|     33 |  912 | `}` |
|      - |  913 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - |  914 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - |  915 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - |  916 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    112 |  917 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    112 |  918 | `	unsigned char c = p[0];` |
|    112 |  919 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    112 |  920 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|    110 |  921 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     56 |  922 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     54 |  923 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     54 |  924 | `		return 2;` |
|      - |  925 | `	}` |
|     56 |  926 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - |  927 | `		sxu32 cp;` |
|     47 |  928 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 |  929 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 |  930 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 |  931 | `		*pCp = cp;` |
|     29 |  932 | `		return 3;` |
|      - |  933 | `	}` |
|     10 |  934 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - |  935 | `		sxu32 cp;` |
|      5 |  936 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 |  937 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 |  938 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 |  939 | `		*pCp = cp;` |
|      5 |  940 | `		return 4;` |
|      - |  941 | `	}` |
|      6 |  942 | `	return 0;                                /* 0xF5-0xFF */` |
|     57 |  943 | `}` |
|      - |  944 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - |  945 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - |  946 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - |  947 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - |  948 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - |  949 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - |  950 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - |  951 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - |  952 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - |  953 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 |  954 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 |  955 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|      - |  956 | `	/* filter_var's FULL_SPECIAL_CHARS has no charset argument: php runs it in the` |
|      - |  957 | `	 * default charset. */` |
|     25 |  958 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/,PH7_HTML_CS_UTF8);` |
|     25 |  959 | `}` |
|      - |  960 | `/* ---------------------------------------------------------------------------` |
|      - |  961 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - |  962 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - |  963 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - |  964 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - |  965 | ` * ------------------------------------------------------------------------ */` |
|      - |  966 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - |  967 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|   3583 |  968 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|   3583 |  969 | `	sxu8 *z = (sxu8 *)zBuf;` |
|   3583 |  970 | `	SX_WRITE_UTF8(z,cp);` |
|   3583 |  971 | `	return (int)(z - (sxu8 *)zBuf);` |
|      3 |  972 | `}` |
|      - |  973 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - |  974 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - |  975 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - |  976 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - |  977 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - |  978 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|    165 |  979 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|    165 |  980 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|    165 |  981 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|    161 |  982 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|    159 |  983 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|    159 |  984 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|    133 |  985 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|    131 |  986 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|    105 |  987 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 |  988 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - |  989 | `	}` |
|    105 |  990 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 |  991 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 |  992 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 |  993 | `	}` |
|    105 |  994 | `	return 1;` |
|     84 |  995 | `}` |
|      - |  996 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - |  997 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - |  998 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - |  999 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|     66 | 1000 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|     66 | 1001 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|     66 | 1002 | `	return HtmlCpAllowed(cp,iFlags);` |
|     34 | 1003 | `}` |
|      - | 1004 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 1005 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 1006 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 1007 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 1008 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 1009 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 1010 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 1011 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 1012 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 1013 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 1014 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 1015 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 1016 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 1017 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 1018 | `	return 1;` |
|      5 | 1019 | `}` |
|      - | 1020 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 1021 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 1022 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 1023 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 1024 | ` * start a new sequence is left for the next round. */` |
|      5 | 1025 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 1026 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 1027 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 1028 | `	unsigned char c = p[0];` |
|     15 | 1029 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 1030 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 1031 | `	if( c < 0xE0 ){` |
|      3 | 1032 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 1033 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 1034 | `	}` |
|     11 | 1035 | `	if( c < 0xF0 ){` |
|     11 | 1036 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 1037 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 1038 | `		}` |
|      9 | 1039 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 1040 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 1041 | `		return 3;` |
|      - | 1042 | `	}` |
|    ! 0 | 1043 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 1044 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 1045 | `	}` |
|    ! 0 | 1046 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 1047 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 1048 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 1049 | `	return 4;` |
|      8 | 1050 | `}` |
|      - | 1051 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 1052 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 1053 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 1054 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 1055 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 1056 | `};` |
|      - | 1057 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 1058 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 1059 | ` * HTML 4.01 table (documented divergence). */` |
|    126 | 1060 | `static int HtmlDocHasNamedTable(int iDoc){` |
|    126 | 1061 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      2 | 1062 | `}` |
|      - | 1063 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 1064 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 1065 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 1066 | ` * whichever function the requested table belongs to. */` |
|     61 | 1067 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     61 | 1068 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     53 | 1069 | `		return "&#039;";` |
|      - | 1070 | `	}` |
|      9 | 1071 | `	return "&apos;";` |
|     32 | 1072 | `}` |
|      - | 1073 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 1074 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 1075 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 1076 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 1077 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 1078 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 1079 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 1080 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 1081 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 1082 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 1083 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 1084 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    218 | 1085 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      3 | 1086 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    221 | 1087 | `	int nAvail = (int)(zEnd - z);` |
|    221 | 1088 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 1089 | `	sxu32 n;` |
|    221 | 1090 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    215 | 1091 | `	if( z[1] == '#' ){` |
|      - | 1092 | `		/* Numeric reference */` |
|    107 | 1093 | `		sxu32 cp = 0;` |
|    107 | 1094 | `		int i = 2, bHex = 0, nDig = 0;` |
|    107 | 1095 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    383 | 1096 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 1097 | `			int v;` |
|    271 | 1098 | `			unsigned char c = z[i];` |
|    271 | 1099 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     22 | 1100 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     22 | 1101 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 1102 | `			else { return 0; }` |
|      - | 1103 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 1104 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    271 | 1105 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    271 | 1106 | `			nDig++;` |
|    137 | 1107 | `		}` |
|    115 | 1108 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|    115 | 1109 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    101 | 1110 | `		if( !bFull ){` |
|      - | 1111 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 1112 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 1113 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 1114 | `		}` |
|     93 | 1115 | `		*pCp = cp;` |
|     93 | 1116 | `		*pnConsumed = i + 1;` |
|     93 | 1117 | `		return 1;` |
|      - | 1118 | `	}` |
|      - | 1119 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 1120 | `	 * else can bail out before touching the tables. */` |
|    111 | 1121 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    413 | 1122 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    375 | 1123 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    337 | 1124 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     67 | 1125 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     67 | 1126 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     67 | 1127 | `			return 1;` |
|      - | 1128 | `		}` |
|    138 | 1129 | `	}` |
|     40 | 1130 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 1131 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 1132 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 1133 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 1134 | `		 * for ~96% of rows. */` |
|   5340 | 1135 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 1136 | `			sxu32 nEnt;` |
|   5326 | 1137 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    188 | 1138 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    188 | 1139 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|     22 | 1140 | `				*pCp = aHtml401Ent[n].cp;` |
|     22 | 1141 | `				*pnConsumed = (int)nEnt;` |
|     22 | 1142 | `				return 1;` |
|      - | 1143 | `			}` |
|     85 | 1144 | `		}` |
|      7 | 1145 | `	}` |
|     20 | 1146 | `	return 0;` |
|    113 | 1147 | `}` |
|      - | 1148 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 1149 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 1150 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 1151 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 1152 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|    166 | 1153 | `PH7_PRIVATE void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      3 | 1154 | `                       int iFlags,int bAll,int bDoubleEncode,int iCs){` |
|    169 | 1155 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|    169 | 1156 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 1157 | `	const unsigned char *runStart;` |
|    169 | 1158 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 1159 | `	/* ENT_DISALLOWED replaces a character the doctype forbids with U+FFFD — as a` |
|      - | 1160 | `	 * CHARACTER where the charset can hold one, and as the numeric REFERENCE for` |
|      - | 1161 | `	 * it where it cannot, which is every single-byte charset. */` |
|    169 | 1162 | `	const char *zRepl = (iCs == PH7_HTML_CS_LATIN1) ? "&#xFFFD;" : "\xEF\xBF\xBD";` |
|      - | 1163 | `	sxu32 cp;` |
|    169 | 1164 | `	if( iCs == PH7_HTML_CS_UTF8 && (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 1165 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 1166 | `		 * bytes cannot be malformed, so skip them without the decoder.` |
|      - | 1167 | `		 * A single-byte charset has no malformed sequences to find. */` |
|    434 | 1168 | `		while( p < zEnd ){` |
|      - | 1169 | `			int len;` |
|    358 | 1170 | `			if( *p < 0x80 ){ p++; continue; }` |
|     44 | 1171 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     44 | 1172 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     32 | 1173 | `			p += len;` |
|      2 | 1174 | `		}` |
|     78 | 1175 | `		p = (const unsigned char *)zIn;` |
|     38 | 1176 | `	}` |
|    157 | 1177 | `	runStart = p;` |
|    157 | 1178 | `	ph7_result_string(pCtx,"",0);` |
|    669 | 1179 | `	while( p < zEnd ){` |
|    515 | 1180 | `		const char *zEnt = 0;` |
|      - | 1181 | `		int len;` |
|    515 | 1182 | `		if( *p < 0x80 ){` |
|    411 | 1183 | `			len = 1;` |
|    411 | 1184 | `			switch( *p ){` |
|     39 | 1185 | `			case '<': zEnt = "&lt;"; break;` |
|     32 | 1186 | `			case '>': zEnt = "&gt;"; break;` |
|     23 | 1187 | `			case '&':` |
|     49 | 1188 | `				zEnt = "&amp;";` |
|     49 | 1189 | `				if( !bDoubleEncode ){` |
|      - | 1190 | `					sxu32 eCp; int nEat;` |
|     31 | 1191 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 1192 | `						/* A valid existing entity: keep it verbatim. */` |
|     16 | 1193 | `						zEnt = 0;` |
|     16 | 1194 | `						len = nEat;` |
|      7 | 1195 | `					}` |
|     14 | 1196 | `				}` |
|     49 | 1197 | `				break;` |
|     13 | 1198 | `			case '"':` |
|     28 | 1199 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     28 | 1200 | `				break;` |
|     15 | 1201 | `			case '\'':` |
|     32 | 1202 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     30 | 1203 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     14 | 1204 | `				}` |
|     32 | 1205 | `				break;` |
|    120 | 1206 | `			default:` |
|    243 | 1207 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|     29 | 1208 | `					zEnt = zRepl;` |
|     14 | 1209 | `				}` |
|    240 | 1210 | `				break;` |
|      - | 1211 | `			}` |
|    311 | 1212 | `		}else if( iCs == PH7_HTML_CS_LATIN1 ){` |
|      - | 1213 | `			/* Every byte is a character and its value IS the code point. */` |
|     37 | 1214 | `			len = 1;` |
|     37 | 1215 | `			cp = *p;` |
|     37 | 1216 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     19 | 1217 | `				zEnt = FvHtml401Lookup(cp);` |
|      9 | 1218 | `			}` |
|     37 | 1219 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|     11 | 1220 | `				zEnt = zRepl;` |
|      5 | 1221 | `			}` |
|     19 | 1222 | `		}else{` |
|     70 | 1223 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     70 | 1224 | `			if( len == 0 ){` |
|      - | 1225 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 1226 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 1227 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 1228 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 1229 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 1230 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 1231 | `				runStart = p;` |
|     15 | 1232 | `				continue;` |
|      - | 1233 | `			}` |
|     56 | 1234 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     46 | 1235 | `				zEnt = FvHtml401Lookup(cp);` |
|     22 | 1236 | `			}` |
|     56 | 1237 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 1238 | `				zEnt = zRepl;` |
|    ! 0 | 1239 | `			}` |
|      - | 1240 | `		}` |
|    501 | 1241 | `		if( zEnt ){` |
|    227 | 1242 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    227 | 1243 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    227 | 1244 | `			runStart = p + len;` |
|    112 | 1245 | `		}` |
|    501 | 1246 | `		p += len;` |
|      3 | 1247 | `	}` |
|    157 | 1248 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     86 | 1249 | `}` |
|      - | 1250 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 1251 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 1252 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 1253 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 1254 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     96 | 1255 | `PH7_PRIVATE void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      3 | 1256 | `                         int iFlags,int bFull,int iCs){` |
|     99 | 1257 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     99 | 1258 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     99 | 1259 | `	const unsigned char *runStart = p;` |
|     99 | 1260 | `	ph7_result_string(pCtx,"",0);` |
|    631 | 1261 | `	while( p < zEnd ){` |
|      - | 1262 | `		sxu32 cp;` |
|      - | 1263 | `		int nEat;` |
|    577 | 1264 | `		if( *p != '&' ){ p++; continue; }` |
|    203 | 1265 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    168 | 1266 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    163 | 1267 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 1268 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     41 | 1269 | `			p += nEat;` |
|     41 | 1270 | `			continue;` |
|      - | 1271 | `		}` |
|    131 | 1272 | `		if( iCs == PH7_HTML_CS_LATIN1 && cp > 0xFF ){` |
|      - | 1273 | `			/* The charset cannot hold it, so php leaves the entity SOURCE alone —` |
|      - | 1274 | ``			 * `&hearts;` stays `&hearts;` in a Latin-1 document. */`` |
|      5 | 1275 | `			p += nEat;` |
|      5 | 1276 | `			continue;` |
|      - | 1277 | `		}` |
|    127 | 1278 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    127 | 1279 | `		if( iCs == PH7_HTML_CS_LATIN1 ){` |
|     17 | 1280 | `			char zByte = (char)cp;` |
|     17 | 1281 | `			ph7_result_string(pCtx,&zByte,1);` |
|      9 | 1282 | `		}else{` |
|      - | 1283 | `			char zBuf[4];` |
|    111 | 1284 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|    111 | 1285 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 1286 | `		}` |
|    127 | 1287 | `		p += nEat;` |
|    127 | 1288 | `		runStart = p;` |
|      3 | 1289 | `	}` |
|     91 | 1290 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     91 | 1291 | `}` |
|      - | 1292 | `/* Resolve the optional charset argument at apArg[idx] to a PH7_HTML_CS_* code.` |
|      - | 1293 | ` *` |
|      - | 1294 | ` * The argument was SCREENED and then dropped: every charset but UTF-8 warned and` |
|      - | 1295 | `` * was answered in UTF-8, so `htmlentities($s, ENT_QUOTES, 'ISO-8859-1')` — the`` |
|      - | 1296 | ` * ordinary call for a Latin-1 page — answered a table of 253 rows where php` |
|      - | 1297 | ` * answers 101, encoded a Latin-1 byte as a broken UTF-8 sequence, and decoded` |
|      - | 1298 | `` * `&eacute;` to two bytes where php writes one.`` |
|      - | 1299 | ` *` |
|      - | 1300 | ` * ISO-8859-1 is a real charset here now (one byte per character, and its VALUE is` |
|      - | 1301 | ` * the code point). php also supports several other single-byte charsets and the` |
|      - | 1302 | ` * Asian multibyte ones; those stay behind the same UTF-8/Latin-1/ASCII scope cut` |
|      - | 1303 | ` * the mb_ and iconv work draws, and keep php's own unsupported-charset warning. */` |
|    253 | 1304 | `PH7_PRIVATE int HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 1305 | `	const char *zCs;` |
|      - | 1306 | `	int nCs;` |
|    253 | 1307 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return PH7_HTML_CS_UTF8; }` |
|    117 | 1308 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|    117 | 1309 | `	if( nCs == 0 ){ return PH7_HTML_CS_UTF8; } /* "" selects the default charset */` |
|    113 | 1310 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     41 | 1311 | `		return PH7_HTML_CS_UTF8; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 1312 | `	}` |
|      - | 1313 | `	/* php's own alias set for Latin-1, matched the way php matches it. */` |
|     72 | 1314 | `	if( (nCs == 10 && SyStrnicmp(zCs,"ISO-8859-1",10) == 0)` |
|     41 | 1315 | `	 \|\| (nCs == 9  && SyStrnicmp(zCs,"ISO8859-1",9) == 0) ){` |
|     67 | 1316 | `		return PH7_HTML_CS_LATIN1;` |
|      - | 1317 | `	}` |
|     13 | 1318 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      4 | 1319 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|      9 | 1320 | `	return PH7_HTML_CS_UTF8;` |
|    129 | 1321 | `}` |
|      - | 1322 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 1323 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 1324 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 1325 | ` * ordering; 253 entries under the defaults). */` |
|   4827 | 1326 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|   4827 | 1327 | `	ph7_value_string(pValue,zEnt,-1);` |
|   4827 | 1328 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|   4827 | 1329 | `	ph7_value_reset_string_cursor(pValue);` |
|   4827 | 1330 | `}` |
|     47 | 1331 | `PH7_PRIVATE void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags,int iCs){` |
|      - | 1332 | `	ph7_value *pArray,*pValue;` |
|     47 | 1333 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 1334 | `	sxu32 n;` |
|     47 | 1335 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     47 | 1336 | `	pArray = ph7_context_new_array(pCtx);` |
|     47 | 1337 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 1338 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1339 | `		return;` |
|      - | 1340 | `	}` |
|     47 | 1341 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     41 | 1342 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|     19 | 1343 | `	}` |
|     47 | 1344 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     47 | 1345 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 1346 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 1347 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 1348 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|     33 | 1349 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|     15 | 1350 | `	}` |
|     47 | 1351 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     47 | 1352 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     47 | 1353 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 1354 | `		char zKey[8];` |
|   6476 | 1355 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 1356 | `			int nK;` |
|   6450 | 1357 | `			if( iCs == PH7_HTML_CS_LATIN1 ){` |
|      - | 1358 | `				/* One byte per character, and only the characters the charset HAS:` |
|      - | 1359 | `				 * php's Latin-1 table is the 96 rows below U+0100 plus the` |
|      - | 1360 | `				 * specials, 101 in all. */` |
|   2977 | 1361 | `				if( aHtml401Ent[n].cp > 0xFF ){` |
|   1825 | 1362 | `					continue;` |
|      - | 1363 | `				}` |
|   1153 | 1364 | `				zKey[0] = (char)aHtml401Ent[n].cp;` |
|   1153 | 1365 | `				nK = 1;` |
|    577 | 1366 | `			}else{` |
|   3474 | 1367 | `				nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|      - | 1368 | `			}` |
|   4626 | 1369 | `			zKey[nK] = 0;` |
|   4626 | 1370 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|   2314 | 1371 | `		}` |
|     13 | 1372 | `	}` |
|     47 | 1373 | `	ph7_result_value(pCtx,pArray);` |
|     25 | 1374 | `}` |
|     25 | 1375 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 1376 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 1377 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 1378 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 1379 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 1380 | `}` |
|      - | 1381 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 1382 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 1383 | `	int i, runStart = 0;` |
|      5 | 1384 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 1385 | `	for( i=0; i<n; i++ ){` |
|     47 | 1386 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 1387 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 1388 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 1389 | `			runStart = i+1;` |
|      5 | 1390 | `		}` |
|     24 | 1391 | `	}` |
|      5 | 1392 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 1393 | `}` |
|      - | 1394 | `/*` |
|      - | 1395 | ` * php's filter NAME table, in php's own order: what filter_list() answers and` |
|      - | 1396 | ` * what filter_id() looks names up in. Two names share id 513 ("string" and` |
|      - | 1397 | ` * "stripped"), which is why filter_id() is a name->id map and not a bijection.` |
|      - | 1398 | ` */` |
|      - | 1399 | `static const struct FvFilterName {` |
|      - | 1400 | `	const char *zName;` |
|      - | 1401 | `	int iId;` |
|      - | 1402 | `} aFvFilterName[] = {` |
|      - | 1403 | `	{ "int",                257 }, { "boolean",            258 },` |
|      - | 1404 | `	{ "float",              259 }, { "validate_regexp",    272 },` |
|      - | 1405 | `	{ "validate_domain",    277 }, { "validate_url",       273 },` |
|      - | 1406 | `	{ "validate_email",     274 }, { "validate_ip",        275 },` |
|      - | 1407 | `	{ "validate_mac",       276 }, { "string",             513 },` |
|      - | 1408 | `	{ "stripped",           513 }, { "encoded",            514 },` |
|      - | 1409 | `	{ "special_chars",      515 }, { "full_special_chars", 522 },` |
|      - | 1410 | `	{ "unsafe_raw",         516 }, { "email",              517 },` |
|      - | 1411 | `	{ "url",                518 }, { "number_int",         519 },` |
|      - | 1412 | `	{ "number_float",       520 }, { "add_slashes",        523 },` |
|      - | 1413 | `	{ "callback",          1024 }` |
|      - | 1414 | `};` |
|      - | 1415 | `/* The NAME php words a filter's failure with. The first row wins for the two` |
|      - | 1416 | ` * ids that have two names, which is the one php's own table answers. */` |
|     20 | 1417 | `static const char * FvFilterIdName(int iFilter)` |
|      1 | 1418 | `{` |
|      - | 1419 | `	sxu32 n;` |
|     79 | 1420 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFvFilterName) ; ++n ){` |
|     79 | 1421 | `		if( aFvFilterName[n].iId == iFilter ){ return aFvFilterName[n].zName; }` |
|     30 | 1422 | `	}` |
|    ! 0 | 1423 | `	return "unsafe_raw";` |
|     11 | 1424 | `}` |
|      - | 1425 | `/* Is this a filter id php has? php answers a filter_var() with an unknown one` |
|      - | 1426 | ` * with a warning and false, and an unknown one INSIDE an array definition with` |
|      - | 1427 | ` * the warning and then the DEFAULT filter. */` |
|   1742 | 1428 | `static int FvFilterIdExists(int iFilter)` |
|      5 | 1429 | `{` |
|      - | 1430 | `	sxu32 n;` |
|  11969 | 1431 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFvFilterName) ; ++n ){` |
|  11965 | 1432 | `		if( aFvFilterName[n].iId == iFilter ){ return 1; }` |
|   5116 | 1433 | `	}` |
|      5 | 1434 | `	return 0;` |
|    876 | 1435 | `}` |
|      - | 1436 | `/*` |
|      - | 1437 | ` * The superglobal an INPUT_* constant names. Answers 0 for a constant php does` |
|      - | 1438 | ` * not have, where the caller raises php's ValueError under its own name.` |
|      - | 1439 | ` */` |
|      8 | 1440 | `static const char * FvInputSuper(int iType,sxu32 *pnLen)` |
|      1 | 1441 | `{` |
|      9 | 1442 | `	switch( iType ){` |
|    ! 0 | 1443 | `	case 0: *pnLen = (sxu32)sizeof("_POST")-1;   return "_POST";` |
|      5 | 1444 | `	case 1: *pnLen = (sxu32)sizeof("_GET")-1;    return "_GET";` |
|    ! 0 | 1445 | `	case 2: *pnLen = (sxu32)sizeof("_COOKIE")-1; return "_COOKIE";` |
|    ! 0 | 1446 | `	case 4: *pnLen = (sxu32)sizeof("_ENV")-1;    return "_ENV";` |
|    ! 0 | 1447 | `	case 5: *pnLen = (sxu32)sizeof("_SERVER")-1; return "_SERVER";` |
|      4 | 1448 | `	default: break;` |
|      - | 1449 | `	}` |
|      5 | 1450 | `	*pnLen = 0;` |
|      5 | 1451 | `	return 0;` |
|      5 | 1452 | `}` |
|      - | 1453 | `/*` |
|      - | 1454 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 1455 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 1456 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 1457 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 1458 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 1459 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 1460 | ` */` |
|   1764 | 1461 | `static int FvApplyFilterRaw(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 1462 | `                            int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 1463 | `                            ph7_value *pDefault,const char *zFunc)` |
|      5 | 1464 | `{` |
|   1769 | 1465 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 1466 | `	const char *zVal; int nVal, rc;` |
|      - | 1467 | `	/* An array/object input fails every scalar filter. */` |
|   1769 | 1468 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|   1769 | 1469 | `	if( iFilter == FV_CALLBACK ){` |
|      - | 1470 | ``		/* php's callback filter: `options` IS the callable (not an array of`` |
|      - | 1471 | `		 * options), it is handed the value, and whatever it answers is the` |
|      - | 1472 | `		 * result. A missing or unusable one is a TypeError, and the value` |
|      - | 1473 | `		 * becomes null. */` |
|      - | 1474 | `		ph7_value *pRes;` |
|      - | 1475 | `		char zReason[256];` |
|      - | 1476 | `		/* the callable SCREEN, not the screen's own message: php words this one` |
|      - | 1477 | `		 * itself ("Option must be a valid callback") */` |
|     27 | 1478 | `		if( pOpts==0 \|\| PH7_VmCallableReason(pCtx->pVm,pOpts,zReason,sizeof(zReason))!=0 ){` |
|      7 | 1479 | `			ph7_result_null(pCtx);` |
|     10 | 1480 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      3 | 1481 | `				"%s(): Option must be a valid callback",zFunc);` |
|      - | 1482 | `		}` |
|     21 | 1483 | `		pRes = ph7_context_new_scalar(pCtx);` |
|     21 | 1484 | `		if( pRes==0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     21 | 1485 | `		if( PH7_VmCallUserFunction(pCtx->pVm,pOpts,1,&pInput,pRes)==SXRET_OK ){` |
|     19 | 1486 | `			ph7_result_value(pCtx,pRes);` |
|     10 | 1487 | `		}else{` |
|      3 | 1488 | `			ph7_result_null(pCtx);` |
|      - | 1489 | `		}` |
|     21 | 1490 | `		ph7_context_release_value(pCtx,pRes);` |
|     21 | 1491 | `		return PH7_OK;` |
|      - | 1492 | `	}` |
|   1743 | 1493 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|   1743 | 1494 | `	switch( iFilter ){` |
|     79 | 1495 | `	case FV_VALIDATE_INT: {` |
|      - | 1496 | `		ph7_int64 v;` |
|    166 | 1497 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|    103 | 1498 | `		if( pOpts ){` |
|     22 | 1499 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|     22 | 1500 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      - | 1501 | `			/* Read through a COPY: ph7_value_to_int64() would rewrite the entry in` |
|      - | 1502 | `			 * the caller's own $options array (php's zval_get_long() does not). */` |
|     22 | 1503 | `			if( pMin && v<PH7_ValuePeekInt64(pMin) ){ goto fail; }` |
|     16 | 1504 | `			if( pMax && v>PH7_ValuePeekInt64(pMax) ){ goto fail; }` |
|      5 | 1505 | `		}` |
|     93 | 1506 | `		ph7_result_int64(pCtx,v);` |
|     93 | 1507 | `		return PH7_OK;` |
|      - | 1508 | `	}` |
|     67 | 1509 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 1510 | `		double d;` |
|    136 | 1511 | `		int decSep = '.';` |
|    136 | 1512 | `		const char *zSep = "',."; int nSep = 3;` |
|      - | 1513 | `		/* php reads "decimal"/"thousand" only when the option IS a string — an int,` |
|      - | 1514 | `		 * a bool, null or an array leaves the defaults in place rather than being` |
|      - | 1515 | `		 * cast — and rejects a decimal that is not exactly one byte, or an empty` |
|      - | 1516 | `		 * separator set, with a ValueError naming the calling function. */` |
|    136 | 1517 | `		if( pOpts ){` |
|     37 | 1518 | `			ph7_value *pDec = ph7_array_fetch(pOpts,"decimal",(int)sizeof("decimal")-1);` |
|     37 | 1519 | `			ph7_value *pSep = ph7_array_fetch(pOpts,"thousand",(int)sizeof("thousand")-1);` |
|     37 | 1520 | `			if( pDec && ph7_value_is_string(pDec) ){` |
|     13 | 1521 | `				int nDec; const char *zDec = ph7_value_to_string(pDec,&nDec); /* already a string: no conversion */` |
|     13 | 1522 | `				if( nDec!=1 ){` |
|      7 | 1523 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|      2 | 1524 | `						"%s(): \"decimal\" option must be one character long",zFunc);` |
|      - | 1525 | `				}` |
|      9 | 1526 | `				decSep = (unsigned char)zDec[0];` |
|      4 | 1527 | `			}` |
|     33 | 1528 | `			if( pSep && ph7_value_is_string(pSep) ){` |
|     11 | 1529 | `				zSep = ph7_value_to_string(pSep,&nSep);` |
|     11 | 1530 | `				if( nSep<1 ){` |
|      4 | 1531 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 1532 | `						"%s(): \"thousand\" option must not be empty",zFunc);` |
|      - | 1533 | `				}` |
|      4 | 1534 | `			}` |
|     15 | 1535 | `		}` |
|    133 | 1536 | `		if( !FvValidateFloat(zVal,nVal,iFlags,decSep,zSep,nSep,&d) ){ goto fail; }` |
|      - | 1537 | `		/* php's range check is the same one the int filter carries, read as a` |
|      - | 1538 | `		 * double (a non-numeric option casts to 0.0, php's own zval_get_double). */` |
|     82 | 1539 | `		if( pOpts ){` |
|     25 | 1540 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|     25 | 1541 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|     25 | 1542 | `			if( pMin && d<PH7_ValuePeekReal(pMin) ){ goto fail; }` |
|     21 | 1543 | `			if( pMax && d>PH7_ValuePeekReal(pMax) ){ goto fail; }` |
|      9 | 1544 | `		}` |
|     76 | 1545 | `		ph7_result_double(pCtx,d);` |
|     76 | 1546 | `		return PH7_OK;` |
|      - | 1547 | `	}` |
|     17 | 1548 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 1549 | `		int b;` |
|     37 | 1550 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     27 | 1551 | `		ph7_result_bool(pCtx,b);` |
|     27 | 1552 | `		return PH7_OK;` |
|      - | 1553 | `	}` |
|    565 | 1554 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     37 | 1555 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|    204 | 1556 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(pCtx,zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|    146 | 1557 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|    323 | 1558 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|      5 | 1559 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 1560 | `#ifdef PH7_ENABLE_PCRE` |
|     12 | 1561 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|     12 | 1562 | `		const char *zRe; int nRe, matched = 0;` |
|     12 | 1563 | `		if( pRe==0 ){` |
|      4 | 1564 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 1565 | `				"%s(): \"regexp\" option is missing",zFunc);` |
|      - | 1566 | `		}` |
|      9 | 1567 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      9 | 1568 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      5 | 1569 | `		goto pass;` |
|      - | 1570 | `#else` |
|      - | 1571 | `		goto fail;` |
|      - | 1572 | `#endif` |
|      - | 1573 | `	}` |
|     26 | 1574 | `	case FV_SANITIZE_STRING:       FvSanitizeStripString(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|      9 | 1575 | `	case FV_SANITIZE_ENCODED:      FvSanitizeEncoded(pCtx,zVal,nVal,iFlags);     return PH7_OK;` |
|      7 | 1576 | `	case FV_SANITIZE_ADD_SLASHES:  FvSanitizeAddSlashes(pCtx,zVal,nVal);         return PH7_OK;` |
|      6 | 1577 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 1578 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     16 | 1579 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 1580 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 1581 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 1582 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     26 | 1583 | `	case FV_DEFAULT:` |
|      - | 1584 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 1585 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. php` |
|      - | 1586 | `		 * takes that branch only for a NON-empty value, so an empty one falls to` |
|      - | 1587 | `		 * the EMPTY_STRING_NULL rule either way. */` |
|     56 | 1588 | `		if( nVal>0 && (iFlags & FV_FLAG_STRING_MASK) ){` |
|     15 | 1589 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 1590 | `			return PH7_OK;` |
|      - | 1591 | `		}` |
|     42 | 1592 | `		if( nVal==0 && (iFlags & FV_FLAG_EMPTY_STRING_NULL) ){` |
|      3 | 1593 | `			ph7_result_null(pCtx);` |
|      3 | 1594 | `			return PH7_OK;` |
|      - | 1595 | `		}` |
|     40 | 1596 | `		goto pass;` |
|      1 | 1597 | `	default:` |
|      - | 1598 | `		/* php's php_zval_filter falls back to the DEFAULT filter for an id it` |
|      - | 1599 | `		 * does not have; the WARNING is raised by the caller that knows which` |
|      - | 1600 | `		 * argument named it. */` |
|      3 | 1601 | `		goto pass;` |
|      - | 1602 | `	}` |
|    360 | 1603 | `fail:` |
|    725 | 1604 | `	if( iFlags & FV_THROW_ON_FAILURE ){` |
|      - | 1605 | `		/* php's message names the FILTER and the value it was given, and the` |
|      - | 1606 | ``		 * `default` option does not suppress it. */`` |
|      - | 1607 | `		ph7_value sTmp;` |
|      - | 1608 | `		int nGiven; const char *zGiven;` |
|     21 | 1609 | `		PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|     21 | 1610 | `		zGiven = ph7_value_to_string(PH7_ValuePeek(pInput,&sTmp),&nGiven);` |
|     21 | 1611 | `		ph7_result_null(pCtx);` |
|     31 | 1612 | `		rc = PH7_VmThrowException(pCtx,"Filter\\FilterFailedException",` |
|      - | 1613 | `			"filter validation failed: filter %s not satisfied by '%.*s'",` |
|     10 | 1614 | `			FvFilterIdName(iFilter),nGiven,zGiven);` |
|     21 | 1615 | `		PH7_MemObjRelease(&sTmp);` |
|     21 | 1616 | `		return rc;` |
|      - | 1617 | `	}` |
|    705 | 1618 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    695 | 1619 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    685 | 1620 | `	else { ph7_result_bool(pCtx,0); }` |
|    705 | 1621 | `	return PH7_OK;` |
|    359 | 1622 | `pass: /* validation passed: return the (string) input unchanged */` |
|    722 | 1623 | `	ph7_result_string(pCtx,zVal,nVal);` |
|    722 | 1624 | `	return PH7_OK;` |
|    887 | 1625 | `}` |
|      - | 1626 | `/*` |
|      - | 1627 | ` * php applies the "default" option by looking at what the filter ANSWERED, not` |
|      - | 1628 | ` * at whether it failed: any false (or, under NULL_ON_FAILURE, any null) is` |
|      - | 1629 | `` * replaced. So `['default' => 'D']` replaces the boolean filter's legitimate`` |
|      - | 1630 | ` * FALSE too, which is the one place the two readings differ.` |
|      - | 1631 | ` */` |
|   1764 | 1632 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 1633 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 1634 | `                         ph7_value *pDefault,const char *zFunc)` |
|      5 | 1635 | `{` |
|   1769 | 1636 | `	int rc = FvApplyFilterRaw(pCtx,pInput,iFilter,iFlags,pOpts,pDefault,zFunc);` |
|   1769 | 1637 | `	if( rc==PH7_OK && pDefault && pCtx->pRet ){` |
|     19 | 1638 | `		int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|     19 | 1639 | `		ph7_value *pRet = pCtx->pRet;` |
|     28 | 1640 | `		if( bNull ? ph7_value_is_null(pRet)` |
|     16 | 1641 | `		          : (ph7_value_is_bool(pRet) && pRet->x.iVal==0) ){` |
|      3 | 1642 | `			ph7_result_value(pCtx,pDefault);` |
|      1 | 1643 | `		}` |
|     10 | 1644 | `	}` |
|   1771 | 1645 | `	return rc;` |
|      5 | 1646 | `}` |
|      - | 1647 | `/*` |
|      - | 1648 | ` * Filter one value into pOut instead of into the call's own result slot. The` |
|      - | 1649 | ` * filters write through ph7_result_xxx(), so the target is switched by pointing` |
|      - | 1650 | ` * the context's return slot at pOut for the duration.` |
|      - | 1651 | ` */` |
|     86 | 1652 | `static int FvApplyFilterInto(ph7_context *pCtx,ph7_value *pIn,ph7_value *pOut,` |
|      - | 1653 | `                             int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 1654 | `                             ph7_value *pDefault,const char *zFunc)` |
|      2 | 1655 | `{` |
|     88 | 1656 | `	ph7_value *pSaved = pCtx->pRet;` |
|      - | 1657 | `	int rc;` |
|     88 | 1658 | `	pCtx->pRet = pOut;` |
|     88 | 1659 | `	rc = FvApplyFilter(pCtx,pIn,iFilter,iFlags,pOpts,pDefault,zFunc);` |
|     88 | 1660 | `	pCtx->pRet = pSaved;` |
|     88 | 1661 | `	return rc;` |
|      2 | 1662 | `}` |
|      - | 1663 | `/*` |
|      - | 1664 | ` * php's recursive walk for an ARRAY input: every element is filtered under the` |
|      - | 1665 | ` * same filter and flags, KEYS are kept, and a nested array is walked rather` |
|      - | 1666 | ` * than failed. (php stops at a self-referencing node; the depth cap here is` |
|      - | 1667 | ` * what stands in for its recursion mark.)` |
|      - | 1668 | ` */` |
|      - | 1669 | `typedef struct FvArrayWalk FvArrayWalk;` |
|      - | 1670 | `struct FvArrayWalk {` |
|      - | 1671 | `	ph7_context *pCtx;` |
|      - | 1672 | `	ph7_value *pOut;` |
|      - | 1673 | `	int iFilter, iFlags, iDepth;` |
|      - | 1674 | `	ph7_value *pOpts, *pDefault;` |
|      - | 1675 | `	const char *zFunc;` |
|      - | 1676 | `	int rc;` |
|      - | 1677 | `};` |
|      - | 1678 | `static int FvArrayWalker(ph7_value *pKey,ph7_value *pData,void *pUserData);` |
|     44 | 1679 | `static int FvFilterArrayInto(ph7_context *pCtx,ph7_value *pIn,ph7_value *pOut,` |
|      - | 1680 | `                             int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 1681 | `                             ph7_value *pDefault,const char *zFunc,int iDepth)` |
|      2 | 1682 | `{` |
|      - | 1683 | `	FvArrayWalk sWalk;` |
|     46 | 1684 | `	sWalk.pCtx = pCtx; sWalk.pOut = pOut;` |
|     46 | 1685 | `	sWalk.iFilter = iFilter; sWalk.iFlags = iFlags; sWalk.iDepth = iDepth;` |
|     46 | 1686 | `	sWalk.pOpts = pOpts; sWalk.pDefault = pDefault; sWalk.zFunc = zFunc;` |
|     46 | 1687 | `	sWalk.rc = PH7_OK;` |
|     46 | 1688 | `	ph7_array_walk(pIn,FvArrayWalker,&sWalk);` |
|     46 | 1689 | `	return sWalk.rc;` |
|      2 | 1690 | `}` |
|      - | 1691 | `/* The walk is C-recursive, as php's is. php stops at a node it has already` |
|      - | 1692 | ` * entered (its recursion mark); this cap is the same idea with a number on it,` |
|      - | 1693 | ` * set to the engine's usual nesting bound so no realistic input reaches it —` |
|      - | 1694 | ` * and a node AT the cap is copied through unfiltered rather than dropped, which` |
|      - | 1695 | ` * is what php does with the node its mark stops at. */` |
|      - | 1696 | `#define FV_MAX_ARRAY_DEPTH 512` |
|     88 | 1697 | `static int FvArrayWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      2 | 1698 | `{` |
|     90 | 1699 | `	FvArrayWalk *pWalk = (FvArrayWalk *)pUserData;` |
|     90 | 1700 | `	ph7_context *pCtx = pWalk->pCtx;` |
|      - | 1701 | `	ph7_value *pElem;` |
|      - | 1702 | `	int rc;` |
|     90 | 1703 | `	if( ph7_value_is_array(pData) ){` |
|     11 | 1704 | `		if( pWalk->iDepth >= FV_MAX_ARRAY_DEPTH ){` |
|    ! 0 | 1705 | `			ph7_array_add_elem(pWalk->pOut,pKey,pData);` |
|    ! 0 | 1706 | `			return PH7_OK;` |
|      - | 1707 | `		}` |
|     11 | 1708 | `		pElem = ph7_context_new_array(pCtx);` |
|     11 | 1709 | `		if( pElem == 0 ){ return PH7_OK; }` |
|     16 | 1710 | `		rc = FvFilterArrayInto(pCtx,pData,pElem,pWalk->iFilter,pWalk->iFlags,` |
|     10 | 1711 | `		                       pWalk->pOpts,pWalk->pDefault,pWalk->zFunc,pWalk->iDepth+1);` |
|      6 | 1712 | `	}else{` |
|     80 | 1713 | `		pElem = ph7_context_new_scalar(pCtx);` |
|     80 | 1714 | `		if( pElem == 0 ){ return PH7_OK; }` |
|    119 | 1715 | `		rc = FvApplyFilterInto(pCtx,pData,pElem,pWalk->iFilter,pWalk->iFlags,` |
|     39 | 1716 | `		                       pWalk->pOpts,pWalk->pDefault,pWalk->zFunc);` |
|      - | 1717 | `	}` |
|     90 | 1718 | `	ph7_array_add_elem(pWalk->pOut,pKey,pElem);` |
|     90 | 1719 | `	ph7_context_release_value(pCtx,pElem);` |
|     90 | 1720 | `	if( rc != PH7_OK ){` |
|      3 | 1721 | `		pWalk->rc = rc;` |
|      3 | 1722 | `		return SXERR_ABORT; /* a filter threw: stop the walk */` |
|      - | 1723 | `	}` |
|     88 | 1724 | `	return PH7_OK;` |
|     46 | 1725 | `}` |
|      - | 1726 | `/*` |
|      - | 1727 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 1728 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 1729 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 1730 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 1731 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 1732 | ` *` |
|      - | 1733 | ` * php's own rule for the flags: whichever spelling carries them, a set that` |
|      - | 1734 | ` * names neither REQUIRE_ARRAY nor FORCE_ARRAY gets REQUIRE_SCALAR added -- so` |
|      - | 1735 | ` * "no flags at all" means "an array input is a failure", and asking for one of` |
|      - | 1736 | ` * the array shapes is what turns that off.` |
|      - | 1737 | ` */` |
|   1722 | 1738 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 1739 | `                              int *piFilter,int *piFlags,` |
|      - | 1740 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      5 | 1741 | `{` |
|   1727 | 1742 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|   1727 | 1743 | `	if( nArg>iBase+1 ){` |
|   1301 | 1744 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|    138 | 1745 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"filter",(int)sizeof("filter")-1);` |
|    138 | 1746 | `			if( pF ){ *piFilter = (int)PH7_ValuePeekInt64(pF); }` |
|    138 | 1747 | `			pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|    138 | 1748 | `			if( pF ){` |
|     60 | 1749 | `				*piFlags = (int)PH7_ValuePeekInt64(pF);` |
|     60 | 1750 | `				if( (*piFlags & (FV_REQUIRE_ARRAY\|FV_FORCE_ARRAY))==0 ){ *piFlags \|= FV_REQUIRE_SCALAR; }` |
|     28 | 1751 | `			}` |
|    138 | 1752 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|    138 | 1753 | `			if( *ppOpts && *piFilter == FV_CALLBACK ){` |
|      - | 1754 | `				/* the CALLBACK filter's "options" IS the callable, and php clears` |
|      - | 1755 | `				 * the shape flags for it */` |
|     23 | 1756 | `				*piFlags = 0;` |
|     12 | 1757 | `			}else{` |
|    116 | 1758 | `				if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|    116 | 1759 | `				if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|      - | 1760 | `			}` |
|     71 | 1761 | `		}else{` |
|   1167 | 1762 | `			*piFlags = (int)PH7_ValuePeekInt64(apArg[iBase+1]);` |
|   1167 | 1763 | `			if( (*piFlags & (FV_REQUIRE_ARRAY\|FV_FORCE_ARRAY))==0 ){ *piFlags \|= FV_REQUIRE_SCALAR; }` |
|      - | 1764 | `		}` |
|    648 | 1765 | `	}` |
|   1727 | 1766 | `}` |
|      - | 1767 | `/*` |
|      - | 1768 | ` * php refuses the two failure modes together, and words it against the argument` |
|      - | 1769 | ` * that carried them.` |
|      - | 1770 | ` */` |
|   1722 | 1771 | `static int FvCheckFailureFlags(ph7_context *pCtx,int iFlags,const char *zFunc,int iArgNo,` |
|      - | 1772 | `                               const char *zArgName)` |
|      5 | 1773 | `{` |
|   1727 | 1774 | `	if( (iFlags & FV_NULL_ON_FAILURE) && (iFlags & FV_THROW_ON_FAILURE) ){` |
|      3 | 1775 | `		ph7_result_null(pCtx);` |
|      4 | 1776 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1777 | `			"%s(): Argument #%d ($%s) cannot use both FILTER_NULL_ON_FAILURE and FILTER_THROW_ON_FAILURE",` |
|      1 | 1778 | `			zFunc,iArgNo,zArgName);` |
|      - | 1779 | `	}` |
|   1725 | 1780 | `	return PH7_OK;` |
|    866 | 1781 | `}` |
|      - | 1782 | `/*` |
|      - | 1783 | `` * php's `array\|int $options`: an array or an int, and nothing else. The shared`` |
|      - | 1784 | `` * type screen leaves a union with an `array` arm alone (php words those from`` |
|      - | 1785 | ` * the builtin's own check), so this is that check.` |
|      - | 1786 | ` */` |
|   1334 | 1787 | `static int FvCheckOptionsArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNo,` |
|      - | 1788 | `                             const char *zArgName)` |
|      5 | 1789 | `{` |
|      - | 1790 | `	char zGiven[64];` |
|   1334 | 1791 | `	if( pArg==0 \|\| ph7_value_is_array(pArg) \|\| ph7_value_is_int(pArg)` |
|    598 | 1792 | `	 \|\| ph7_value_is_bool(pArg) \|\| ph7_value_is_null(pArg) ){` |
|   1329 | 1793 | `		return PH7_OK;` |
|      - | 1794 | `	}` |
|     11 | 1795 | `	if( ph7_value_is_float(pArg) \|\| PH7_MemObjStringIsNumeric(pArg) ){` |
|      3 | 1796 | `		return PH7_OK; /* weak mode coerces a number the way php's ZPP does */` |
|      - | 1797 | `	}` |
|      9 | 1798 | `	ph7_result_null(pCtx);` |
|     13 | 1799 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1800 | `		"%s(): Argument #%d ($%s) must be of type array\|int, %s given",` |
|      4 | 1801 | `		zFunc,iArgNo,zArgName,VmValueGivenName(pArg,zGiven,sizeof(zGiven)));` |
|    672 | 1802 | `}` |
|      - | 1803 | `/*` |
|      - | 1804 | ` * php's php_filter_call: what the ARRAY shape flags decide before any filter` |
|      - | 1805 | ` * runs. An array input is refused under REQUIRE_SCALAR and walked otherwise; a` |
|      - | 1806 | ` * scalar input is refused under REQUIRE_ARRAY; and FORCE_ARRAY wraps whatever` |
|      - | 1807 | ` * the filter answered in a one-element list.` |
|      - | 1808 | ` */` |
|   1734 | 1809 | `static int FvFilterCall(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 1810 | `                        int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 1811 | `                        ph7_value *pDefault,const char *zFunc)` |
|      5 | 1812 | `{` |
|   1739 | 1813 | `	if( ph7_value_is_array(pInput) ){` |
|      - | 1814 | `		ph7_value *pOut;` |
|      - | 1815 | `		int rc;` |
|     47 | 1816 | `		if( iFlags & FV_REQUIRE_SCALAR ){` |
|     12 | 1817 | `			if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_null(pCtx); }` |
|     10 | 1818 | `			else{ ph7_result_bool(pCtx,0); }` |
|     12 | 1819 | `			return PH7_OK;` |
|      - | 1820 | `		}` |
|     36 | 1821 | `		pOut = ph7_context_new_array(pCtx);` |
|     36 | 1822 | `		if( pOut == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|     36 | 1823 | `		rc = FvFilterArrayInto(pCtx,pInput,pOut,iFilter,iFlags,pOpts,pDefault,zFunc,0);` |
|     36 | 1824 | `		if( rc == PH7_OK ){ ph7_result_value(pCtx,pOut); }` |
|     36 | 1825 | `		ph7_context_release_value(pCtx,pOut);` |
|     36 | 1826 | `		return rc;` |
|      - | 1827 | `	}` |
|   1695 | 1828 | `	if( iFlags & FV_REQUIRE_ARRAY ){` |
|      5 | 1829 | `		if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_null(pCtx); }` |
|      3 | 1830 | `		else{ ph7_result_bool(pCtx,0); }` |
|      5 | 1831 | `		return PH7_OK;` |
|      - | 1832 | `	}` |
|   1691 | 1833 | `	if( iFlags & FV_FORCE_ARRAY ){` |
|      9 | 1834 | `		ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      9 | 1835 | `		ph7_value *pElem = ph7_context_new_scalar(pCtx);` |
|      - | 1836 | `		int rc;` |
|      9 | 1837 | `		if( pOut == 0 \|\| pElem == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|      9 | 1838 | `		rc = FvApplyFilterInto(pCtx,pInput,pElem,iFilter,iFlags,pOpts,pDefault,zFunc);` |
|      9 | 1839 | `		if( rc == PH7_OK ){` |
|      9 | 1840 | `			ph7_array_add_elem(pOut,0,pElem);` |
|      9 | 1841 | `			ph7_result_value(pCtx,pOut);` |
|      4 | 1842 | `		}` |
|      9 | 1843 | `		ph7_context_release_value(pCtx,pElem);` |
|      9 | 1844 | `		ph7_context_release_value(pCtx,pOut);` |
|      9 | 1845 | `		return rc;` |
|      - | 1846 | `	}` |
|   1683 | 1847 | `	return FvApplyFilter(pCtx,pInput,iFilter,iFlags,pOpts,pDefault,zFunc);` |
|    872 | 1848 | `}` |
|      - | 1849 | `/*` |
|      - | 1850 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 1851 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 1852 | ` */` |
|   1704 | 1853 | `PH7_PRIVATE int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1854 | `{` |
|   1709 | 1855 | `	int iFilter = FV_DEFAULT, iFlags = FV_REQUIRE_SCALAR;` |
|   1709 | 1856 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|   1709 | 1857 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|   1709 | 1858 | `	if( nArg>2 && FvCheckOptionsArg(pCtx,apArg[2],"filter_var",3,"options")!=PH7_OK ){` |
|      5 | 1859 | `		return PH7_EXCEPTION;` |
|      - | 1860 | `	}` |
|   1705 | 1861 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|   1705 | 1862 | `	if( FvCheckFailureFlags(pCtx,iFlags,"filter_var",3,"options")!=PH7_OK ){` |
|      3 | 1863 | `		return PH7_EXCEPTION;` |
|      - | 1864 | `	}` |
|   1703 | 1865 | `	if( !FvFilterIdExists(iFilter) ){` |
|      3 | 1866 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown filter with ID %d",iFilter);` |
|      3 | 1867 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1868 | `		return PH7_OK;` |
|      - | 1869 | `	}` |
|   1701 | 1870 | `	return FvFilterCall(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault,"filter_var");` |
|    857 | 1871 | `}` |
|      - | 1872 | `/*` |
|      - | 1873 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 1874 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 1875 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 1876 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 1877 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 1878 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 1879 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 1880 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 1881 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 1882 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 1883 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 1884 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 1885 | ` *  php's snapshot.` |
|      - | 1886 | ` */` |
|     24 | 1887 | `PH7_PRIVATE int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1888 | `{` |
|     26 | 1889 | `	int iType, iFilter = FV_DEFAULT, iFlags = FV_REQUIRE_SCALAR;` |
|     26 | 1890 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 1891 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     26 | 1892 | `	if( nArg<2 ){` |
|    ! 0 | 1893 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1894 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 1895 | `	}` |
|     26 | 1896 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 1897 | `	switch( iType ){` |
|      3 | 1898 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 1899 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 1900 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 1901 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 1902 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 1903 | `	default:` |
|      3 | 1904 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1905 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 1906 | `	}` |
|     23 | 1907 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 1908 | `	if( nArg>3 && FvCheckOptionsArg(pCtx,apArg[3],"filter_input",4,"options")!=PH7_OK ){` |
|    ! 0 | 1909 | `		return PH7_EXCEPTION;` |
|      - | 1910 | `	}` |
|     23 | 1911 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|     23 | 1912 | `	if( FvCheckFailureFlags(pCtx,iFlags,"filter_input",4,"options")!=PH7_OK ){` |
|    ! 0 | 1913 | `		return PH7_EXCEPTION;` |
|      - | 1914 | `	}` |
|     23 | 1915 | `	if( !FvFilterIdExists(iFilter) ){` |
|    ! 0 | 1916 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 1917 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1918 | `		return PH7_OK;` |
|      - | 1919 | `	}` |
|      - | 1920 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 1921 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 1922 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 1923 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 1924 | `	if( pElem==0 ){` |
|      - | 1925 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 1926 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 1927 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 1928 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 1929 | `		else { ph7_result_null(pCtx); }` |
|     13 | 1930 | `		return PH7_OK;` |
|      - | 1931 | `	}` |
|     11 | 1932 | `	return FvFilterCall(pCtx,pElem,iFilter,iFlags,pOpts,pDefault,"filter_input");` |
|     14 | 1933 | `}` |
|      - | 1934 | `/*` |
|      - | 1935 | ` * filter_list(): every filter NAME php knows, in php's order.` |
|      - | 1936 | ` */` |
|      4 | 1937 | `PH7_PRIVATE int PH7_builtin_filter_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1938 | `{` |
|      - | 1939 | `	ph7_value *pArray, *pVal;` |
|      - | 1940 | `	sxu32 n;` |
|      2 | 1941 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      5 | 1942 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 1943 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      5 | 1944 | `	if( pArray==0 \|\| pVal==0 ){` |
|    ! 0 | 1945 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1946 | `		return PH7_OK;` |
|      - | 1947 | `	}` |
|     89 | 1948 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFvFilterName) ; ++n ){` |
|     85 | 1949 | `		ph7_value_string(pVal,aFvFilterName[n].zName,-1);` |
|     85 | 1950 | `		ph7_array_add_elem(pArray,0,pVal);` |
|     85 | 1951 | `		ph7_value_reset_string_cursor(pVal);` |
|     43 | 1952 | `	}` |
|      5 | 1953 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 1954 | `	ph7_context_release_value(pCtx,pVal);` |
|      5 | 1955 | `	ph7_context_release_value(pCtx,pArray);` |
|      5 | 1956 | `	return PH7_OK;` |
|      3 | 1957 | `}` |
|      - | 1958 | `/*` |
|      - | 1959 | ` * filter_id(string $name): the id behind a filter name, or false. The lookup is` |
|      - | 1960 | ` * php's: exact, case-SENSITIVE, over the same table filter_list() answers.` |
|      - | 1961 | ` */` |
|     48 | 1962 | `PH7_PRIVATE int PH7_builtin_filter_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1963 | `{` |
|      - | 1964 | `	const char *zName; int nName; sxu32 n;` |
|     49 | 1965 | `	if( nArg<1 ){` |
|    ! 0 | 1966 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1967 | `			"filter_id() expects exactly 1 argument, %d given",nArg);` |
|      - | 1968 | `	}` |
|     49 | 1969 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    595 | 1970 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFvFilterName) ; ++n ){` |
|    588 | 1971 | `		if( (int)SyStrlen(aFvFilterName[n].zName)==nName` |
|    329 | 1972 | `		 && SyMemcmp(aFvFilterName[n].zName,zName,(sxu32)nName)==0 ){` |
|     43 | 1973 | `			ph7_result_int(pCtx,aFvFilterName[n].iId);` |
|     43 | 1974 | `			return PH7_OK;` |
|      - | 1975 | `		}` |
|    274 | 1976 | `	}` |
|      7 | 1977 | `	ph7_result_bool(pCtx,0);` |
|      7 | 1978 | `	return PH7_OK;` |
|     25 | 1979 | `}` |
|      - | 1980 | `/*` |
|      - | 1981 | ` * filter_has_var(int $input_type, string $var_name): is the variable there at` |
|      - | 1982 | ` * all, before any filter runs. (Same divergence filter_input() records: php` |
|      - | 1983 | ` * reads the SAPI's startup snapshot of the request variables, PHL the live` |
|      - | 1984 | ` * superglobal.)` |
|      - | 1985 | ` */` |
|      4 | 1986 | `PH7_PRIVATE int PH7_builtin_filter_has_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1987 | `{` |
|      - | 1988 | `	const char *zSuper, *zVar; sxu32 nSuper; int nVar;` |
|      - | 1989 | `	ph7_value *pSuper;` |
|      5 | 1990 | `	if( nArg<2 ){` |
|    ! 0 | 1991 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1992 | `			"filter_has_var() expects exactly 2 arguments, %d given",nArg);` |
|      - | 1993 | `	}` |
|      5 | 1994 | `	zSuper = FvInputSuper(ph7_value_to_int(apArg[0]),&nSuper);` |
|      5 | 1995 | `	if( zSuper==0 ){` |
|      3 | 1996 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1997 | `			"filter_has_var(): Argument #1 ($input_type) must be an INPUT_* constant");` |
|      - | 1998 | `	}` |
|      3 | 1999 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|      3 | 2000 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|      5 | 2001 | `	ph7_result_bool(pCtx,(pSuper && ph7_value_is_array(pSuper)` |
|      2 | 2002 | `	                      && ph7_array_fetch(pSuper,zVar,nVar)) ? 1 : 0);` |
|      3 | 2003 | `	return PH7_OK;` |
|      3 | 2004 | `}` |
|      - | 2005 | `/*` |
|      - | 2006 | ` * The body php's filter_var_array()/filter_input_array() share.` |
|      - | 2007 | ` *` |
|      - | 2008 | ` * A plain int $options is one filter for the WHOLE array (php defaults the` |
|      - | 2009 | ` * shape flag to REQUIRE_ARRAY there, so the array is walked rather than` |
|      - | 2010 | ` * refused). A definition ARRAY is per-key: its keys name the entries to read,` |
|      - | 2011 | ` * and each value is either a filter id or the same {filter, flags, options}` |
|      - | 2012 | ` * array filter_var() takes. A key that is not in the input answers NULL when` |
|      - | 2013 | ` * $add_empty is on, and is left out otherwise.` |
|      - | 2014 | ` */` |
|     32 | 2015 | `static int FvArrayHandler(ph7_context *pCtx,ph7_value *pInput,int nArg,ph7_value **apArg,` |
|      - | 2016 | `                          int iSpecArg,const char *zFunc)` |
|      2 | 2017 | `{` |
|     34 | 2018 | `	int bAddEmpty = 1;` |
|     34 | 2019 | `	ph7_value *pSpec = (nArg>iSpecArg) ? apArg[iSpecArg] : 0;` |
|     34 | 2020 | `	if( pSpec && FvCheckOptionsArg(pCtx,pSpec,zFunc,iSpecArg+1,"options")!=PH7_OK ){` |
|      3 | 2021 | `		return PH7_EXCEPTION;` |
|      - | 2022 | `	}` |
|     31 | 2023 | `	if( nArg>iSpecArg+1 ){ bAddEmpty = PH7_ValuePeekBool(apArg[iSpecArg+1]); }` |
|     31 | 2024 | `	if( pSpec==0 \|\| !ph7_value_is_array(pSpec) ){` |
|      - | 2025 | `		/* one filter over the whole array */` |
|      9 | 2026 | `		int iFilter = pSpec ? (int)PH7_ValuePeekInt64(pSpec) : FV_DEFAULT;` |
|      9 | 2027 | `		if( !FvFilterIdExists(iFilter) ){` |
|    ! 0 | 2028 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 2029 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2030 | `			return PH7_OK;` |
|      - | 2031 | `		}` |
|      9 | 2032 | `		return FvFilterCall(pCtx,pInput,iFilter,FV_REQUIRE_ARRAY,0,0,zFunc);` |
|    ! 0 | 2033 | `	}else{` |
|     23 | 2034 | `		ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - | 2035 | `		ph7_hashmap *pMap;` |
|      - | 2036 | `		ph7_hashmap_node *pNode;` |
|      - | 2037 | `		sxu32 n;` |
|     23 | 2038 | `		if( pOut==0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|     23 | 2039 | `		pMap = (ph7_hashmap *)pSpec->x.pOther;` |
|     23 | 2040 | `		pNode = pMap->pFirst;` |
|     47 | 2041 | `		for( n = 0 ; n < pMap->nEntry && pNode ; ++n, pNode = pNode->pPrev ){` |
|      - | 2042 | `			ph7_value sKey, sElem, *pSrc, *pRes;` |
|      - | 2043 | `			const char *zKey; int nKey;` |
|     29 | 2044 | `			int iFilter = FV_DEFAULT, iFlags = FV_REQUIRE_SCALAR;` |
|     29 | 2045 | `			ph7_value *pOpts = 0, *pDefault = 0;` |
|     29 | 2046 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|     29 | 2047 | `			PH7_MemObjInit(pCtx->pVm,&sElem);` |
|     29 | 2048 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|     29 | 2049 | `			PH7_HashmapExtractNodeValue(pNode,&sElem,FALSE);` |
|     29 | 2050 | `			if( !ph7_value_is_string(&sKey) ){` |
|      3 | 2051 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sElem);` |
|      3 | 2052 | `				ph7_context_release_value(pCtx,pOut);` |
|      5 | 2053 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|      1 | 2054 | `					"%s(): Argument #2 ($options) must contain only string keys",zFunc);` |
|      - | 2055 | `			}` |
|     27 | 2056 | `			zKey = ph7_value_to_string(&sKey,&nKey);` |
|     27 | 2057 | `			if( nKey<1 ){` |
|      3 | 2058 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sElem);` |
|      3 | 2059 | `				ph7_context_release_value(pCtx,pOut);` |
|      4 | 2060 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 2061 | `					"%s(): Argument #2 ($options) cannot contain empty keys",zFunc);` |
|      - | 2062 | `			}` |
|      - | 2063 | `			/* the per-key spelling: a bare filter id, or filter_var()'s own` |
|      - | 2064 | `			 * {filter, flags, options} array */` |
|     25 | 2065 | `			if( ph7_value_is_array(&sElem) ){` |
|     11 | 2066 | `				ph7_value *pF = ph7_array_fetch(&sElem,"filter",(int)sizeof("filter")-1);` |
|     11 | 2067 | `				if( pF ){ iFilter = (int)PH7_ValuePeekInt64(pF); }` |
|     11 | 2068 | `				pF = ph7_array_fetch(&sElem,"flags",(int)sizeof("flags")-1);` |
|     11 | 2069 | `				if( pF ){` |
|      7 | 2070 | `					iFlags = (int)PH7_ValuePeekInt64(pF);` |
|      7 | 2071 | `					if( (iFlags & (FV_REQUIRE_ARRAY\|FV_FORCE_ARRAY))==0 ){ iFlags \|= FV_REQUIRE_SCALAR; }` |
|      3 | 2072 | `				}` |
|     11 | 2073 | `				pOpts = ph7_array_fetch(&sElem,"options",(int)sizeof("options")-1);` |
|     11 | 2074 | `				if( pOpts && iFilter == FV_CALLBACK ){` |
|    ! 0 | 2075 | `					iFlags = 0; /* the callable is the option; php clears the shape flags */` |
|    ! 0 | 2076 | `				}else{` |
|     11 | 2077 | `					if( pOpts && !ph7_value_is_array(pOpts) ){ pOpts = 0; }` |
|     11 | 2078 | `					if( pOpts ){ pDefault = ph7_array_fetch(pOpts,"default",(int)sizeof("default")-1); }` |
|      - | 2079 | `				}` |
|      6 | 2080 | `			}else{` |
|     15 | 2081 | `				iFilter = (int)PH7_ValuePeekInt64(&sElem);` |
|     15 | 2082 | `				if( !FvFilterIdExists(iFilter) ){` |
|      4 | 2083 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      1 | 2084 | `						"Unknown filter with ID %d",iFilter);` |
|      1 | 2085 | `				}` |
|      - | 2086 | `			}` |
|     25 | 2087 | `			pSrc = (pInput && ph7_value_is_array(pInput)) ? ph7_array_fetch(pInput,zKey,nKey) : 0;` |
|     25 | 2088 | `			pRes = ph7_context_new_scalar(pCtx);` |
|     25 | 2089 | `			if( pRes==0 ){ PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sElem); break; }` |
|     25 | 2090 | `			if( pSrc==0 ){` |
|      5 | 2091 | `				if( bAddEmpty ){` |
|      3 | 2092 | `					ph7_value_null(pRes);` |
|      3 | 2093 | `					ph7_array_add_elem(pOut,&sKey,pRes);` |
|      1 | 2094 | `				}` |
|      3 | 2095 | `			}else{` |
|     21 | 2096 | `				ph7_value *pSaved = pCtx->pRet;` |
|      - | 2097 | `				int rc;` |
|     21 | 2098 | `				pCtx->pRet = pRes;` |
|     21 | 2099 | `				rc = FvFilterCall(pCtx,pSrc,iFilter,iFlags,pOpts,pDefault,zFunc);` |
|     21 | 2100 | `				pCtx->pRet = pSaved;` |
|     21 | 2101 | `				if( rc != PH7_OK ){` |
|    ! 0 | 2102 | `					ph7_context_release_value(pCtx,pRes);` |
|    ! 0 | 2103 | `					PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sElem);` |
|    ! 0 | 2104 | `					ph7_context_release_value(pCtx,pOut);` |
|    ! 0 | 2105 | `					return rc;` |
|      - | 2106 | `				}` |
|     21 | 2107 | `				ph7_array_add_elem(pOut,&sKey,pRes);` |
|      - | 2108 | `			}` |
|     25 | 2109 | `			ph7_context_release_value(pCtx,pRes);` |
|     25 | 2110 | `			PH7_MemObjRelease(&sKey);` |
|     25 | 2111 | `			PH7_MemObjRelease(&sElem);` |
|     13 | 2112 | `		}` |
|     19 | 2113 | `		ph7_result_value(pCtx,pOut);` |
|     19 | 2114 | `		ph7_context_release_value(pCtx,pOut);` |
|     19 | 2115 | `		return PH7_OK;` |
|      - | 2116 | `	}` |
|     18 | 2117 | `}` |
|      - | 2118 | `/*` |
|      - | 2119 | ` * filter_var_array($array, $options = FILTER_DEFAULT, $add_empty = true)` |
|      - | 2120 | ` */` |
|     32 | 2121 | `PH7_PRIVATE int PH7_builtin_filter_var_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2122 | `{` |
|      - | 2123 | `	char zGiven[64];` |
|     34 | 2124 | `	if( nArg<1 ){` |
|    ! 0 | 2125 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 2126 | `			"filter_var_array() expects at least 1 argument, %d given",nArg);` |
|      - | 2127 | `	}` |
|     34 | 2128 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|    ! 0 | 2129 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2130 | `			"filter_var_array(): Argument #1 ($array) must be of type array, %s given",` |
|    ! 0 | 2131 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|      - | 2132 | `	}` |
|     34 | 2133 | `	return FvArrayHandler(pCtx,apArg[0],nArg,apArg,1,"filter_var_array");` |
|     18 | 2134 | `}` |
|      - | 2135 | `/*` |
|      - | 2136 | ` * filter_input_array($type, $options = FILTER_DEFAULT, $add_empty = true)` |
|      - | 2137 | ` *  The same handler over an INPUT_* superglobal; a source with no data at all` |
|      - | 2138 | ` *  is php's NULL rather than an empty array.` |
|      - | 2139 | ` */` |
|      6 | 2140 | `PH7_PRIVATE int PH7_builtin_filter_input_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2141 | `{` |
|      - | 2142 | `	const char *zSuper; sxu32 nSuper;` |
|      - | 2143 | `	ph7_value *pSuper;` |
|      8 | 2144 | `	if( nArg<1 ){` |
|    ! 0 | 2145 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 2146 | `			"filter_input_array() expects at least 1 argument, %d given",nArg);` |
|      - | 2147 | `	}` |
|      - | 2148 | `	/* php's ZPP screens the arguments before it looks at the source at all. */` |
|      8 | 2149 | `	if( nArg>1 && FvCheckOptionsArg(pCtx,apArg[1],"filter_input_array",2,"options")!=PH7_OK ){` |
|      3 | 2150 | `		return PH7_EXCEPTION;` |
|      - | 2151 | `	}` |
|      5 | 2152 | `	zSuper = FvInputSuper(ph7_value_to_int(apArg[0]),&nSuper);` |
|      5 | 2153 | `	if( zSuper==0 ){` |
|      3 | 2154 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2155 | `			"filter_input_array(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 2156 | `	}` |
|      3 | 2157 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|      3 | 2158 | `	if( pSuper==0 \|\| !ph7_value_is_array(pSuper) \|\| ph7_array_count(pSuper)<1 ){` |
|      3 | 2159 | `		ph7_result_null(pCtx);` |
|      3 | 2160 | `		return PH7_OK;` |
|      - | 2161 | `	}` |
|    ! 0 | 2162 | `	return FvArrayHandler(pCtx,pSuper,nArg,apArg,1,"filter_input_array");` |
|      5 | 2163 | `}` |
|      - | 2164 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 2165 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 2166 | `/*` |
|      - | 2167 | ` * The incremental half of the parser below: walk a partially-read record and` |
|      - | 2168 | ` * answer whether it ends INSIDE an enclosure, which is how fgetcsv() knows the` |
|      - | 2169 | ` * value contains the newline and the record continues on the next line.` |
|      - | 2170 | ` *` |
|      - | 2171 | ` * It carries its position and state across appended chunks on purpose. Deciding` |
|      - | 2172 | ` * it by re-parsing the whole accumulated record after every line is quadratic,` |
|      - | 2173 | ` * and one stray quote in a large file is exactly the input that triggers it.` |
|      - | 2174 | ` * The trailing line ending PH7_ProcessCsv strips cannot close an enclosure, so` |
|      - | 2175 | ` * this scan ignores it and both agree on every record.` |
|      - | 2176 | ` */` |
|     70 | 2177 | `PH7_PRIVATE void PH7_CsvScanInit(PH7_CsvScan *pScan)` |
|      1 | 2178 | `{` |
|     71 | 2179 | `	pScan->iState = 0;` |
|     71 | 2180 | `	pScan->nPos = 0;` |
|     71 | 2181 | `}` |
|     80 | 2182 | `PH7_PRIVATE int PH7_CsvScanOpen(PH7_CsvScan *pScan,const char *zIn,sxu32 nByte,` |
|      - | 2183 | `	int delim,int encl,int escape)` |
|      1 | 2184 | `{` |
|     81 | 2185 | `	sxu32 i = pScan->nPos;` |
|    547 | 2186 | `	while( i < nByte ){` |
|    467 | 2187 | `		int c = (unsigned char)zIn[i];` |
|    467 | 2188 | `		switch( pScan->iState ){` |
|     66 | 2189 | `		case 0: /* at a field start: whitespace only counts as padding when an` |
|      - | 2190 | `		         * enclosure is what it leads to */` |
|    133 | 2191 | `			if( c != delim && SyisSpace(c) ){` |
|     11 | 2192 | `				i++;` |
|     11 | 2193 | `				continue;` |
|      - | 2194 | `			}` |
|    123 | 2195 | `			if( c == encl ){` |
|     27 | 2196 | `				pScan->iState = 2;` |
|     27 | 2197 | `				i++;` |
|     27 | 2198 | `				continue;` |
|      - | 2199 | `			}` |
|     97 | 2200 | `			if( c == delim ){` |
|      5 | 2201 | `				i++;` |
|      5 | 2202 | `				continue;` |
|      - | 2203 | `			}` |
|     93 | 2204 | `			pScan->iState = 1;` |
|     93 | 2205 | `			continue;` |
|     92 | 2206 | `		case 1: /* unquoted field: an enclosure here is ordinary content */` |
|    185 | 2207 | `			if( c == delim ){` |
|     43 | 2208 | `				pScan->iState = 0;` |
|     21 | 2209 | `			}` |
|    185 | 2210 | `			i++;` |
|    185 | 2211 | `			continue;` |
|     63 | 2212 | `		case 2: /* inside the enclosure */` |
|    127 | 2213 | `			if( c == encl ){` |
|     27 | 2214 | `				if( i + 1 < nByte && (unsigned char)zIn[i+1] == encl ){` |
|      3 | 2215 | `					i += 2;   /* doubled: a literal enclosure */` |
|      3 | 2216 | `					continue;` |
|      - | 2217 | `				}` |
|     25 | 2218 | `				pScan->iState = 3;` |
|     25 | 2219 | `				i++;` |
|     25 | 2220 | `				continue;` |
|      - | 2221 | `			}` |
|    101 | 2222 | `			if( escape != PH7_CSV_NO_ESCAPE && c == escape ){` |
|    ! 0 | 2223 | `				i += (i + 1 < nByte) ? 2 : 1;` |
|    ! 0 | 2224 | `				continue;` |
|      - | 2225 | `			}` |
|    101 | 2226 | `			i++;` |
|    101 | 2227 | `			continue;` |
|     12 | 2228 | `		default: /* past the closing enclosure, up to the delimiter */` |
|     25 | 2229 | `			if( c == delim ){` |
|     21 | 2230 | `				pScan->iState = 0;` |
|     10 | 2231 | `			}` |
|     25 | 2232 | `			i++;` |
|     25 | 2233 | `			continue;` |
|      - | 2234 | `		}` |
|    ! 0 | 2235 | `	}` |
|     81 | 2236 | `	pScan->nPos = i;` |
|     81 | 2237 | `	return pScan->iState == 2;` |
|      1 | 2238 | `}` |
|      - | 2239 | `/*` |
|      - | 2240 | ` * Strip ONE trailing line ending -- "\r\n", "\n" or "\r" -- and answer the` |
|      - | 2241 | ` * length left. php applies it to the whole line before parsing, and again to` |
|      - | 2242 | ` * each UNQUOTED field's own content (which is how a lone "\n" reads back as the` |
|      - | 2243 | ` * empty string while "a\r\rb" keeps both of its carriage returns).` |
|      - | 2244 | ` */` |
|    374 | 2245 | `static int CsvStripEol(const char *zIn,int nByte)` |
|      1 | 2246 | `{` |
|    375 | 2247 | `	if( nByte > 0 && zIn[nByte-1] == '\n' ){` |
|     81 | 2248 | `		nByte--;` |
|     81 | 2249 | `		if( nByte > 0 && zIn[nByte-1] == '\r' ){` |
|      9 | 2250 | `			nByte--;` |
|      5 | 2251 | `		}` |
|    335 | 2252 | `	}else if( nByte > 0 && zIn[nByte-1] == '\r' ){` |
|      5 | 2253 | `		nByte--;` |
|      2 | 2254 | `	}` |
|    375 | 2255 | `	return nByte;` |
|      1 | 2256 | `}` |
|      - | 2257 | `/*` |
|      - | 2258 | ` * Parse one CSV record and append each field to pArray.` |
|      - | 2259 | ` *` |
|      - | 2260 | ` * A port of php's php_fgetcsv rules, derived from the oracle field by field.` |
|      - | 2261 | ` * PH7's tokenizer answered a different record for most inputs that were not` |
|      - | 2262 | ` * already trivial:` |
|      - | 2263 | `` *  - an EMPTY field was dropped along with its delimiter, so `,a` read as one`` |
|      - | 2264 | `` *    column and `a,b,` as two -- every later column shifted;`` |
|      - | 2265 | ` *  - a doubled enclosure was not undoubled, and the closing one was located by` |
|      - | 2266 | `` *    a parity toggle, so `"a""b"` came back with its quoting intact;`` |
|      - | 2267 | ` *  - the field content was TRIMMED of whitespace and NUL bytes by the consumer,` |
|      - | 2268 | `` *    so `" a "` -- quoted precisely to keep those spaces -- lost them;`` |
|      - | 2269 | ` *  - the escape character consumed the byte after it even OUTSIDE an enclosure,` |
|      - | 2270 | ` *    where php gives it no meaning at all.` |
|      - | 2271 | ` * The rules that are not guessable are the ones php's own parser reaches by` |
|      - | 2272 | ` * accident and programs depend on: whitespace before an opening enclosure is` |
|      - | 2273 | ` * skipped (but kept when no enclosure follows), whatever trails a CLOSING` |
|      - | 2274 | `` * enclosure up to the delimiter is APPENDED to the field (`"a"b` is `ab`), an`` |
|      - | 2275 | ` * escape keeps BOTH bytes rather than the escaped one alone, and a record whose` |
|      - | 2276 | ` * whole text is empty is a single NULL field rather than an empty string.` |
|      - | 2277 | ` *` |
|      - | 2278 | ` * *pbOpen (optional) reports that the text ran out inside an enclosure, which is` |
|      - | 2279 | ` * how fgetcsv() knows a quoted newline means the record continues on the next` |
|      - | 2280 | ` * line.` |
|      - | 2281 | ` */` |
|    180 | 2282 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 2283 | `	ph7_value *pArray, /* Fields are appended here */` |
|      - | 2284 | `	const char *zInput, /* Raw input */` |
|      - | 2285 | `	int nByte,  /* Input length */` |
|      - | 2286 | `	int delim,  /* Delimiter */` |
|      - | 2287 | `	int encl,   /* Enclosure */` |
|      - | 2288 | `	int escape, /* Escape character, or PH7_CSV_NO_ESCAPE */` |
|      - | 2289 | `	int *pbOpen /* OUT: the text ended inside an enclosure */` |
|      - | 2290 | `	)` |
|      1 | 2291 | `{` |
|    181 | 2292 | `	ph7_vm *pVm = pArray->pVm;` |
|      - | 2293 | `	SyBlob sField;` |
|      - | 2294 | `	ph7_value sEntry;` |
|    181 | 2295 | `	int nLimit = CsvStripEol(zInput,nByte);` |
|    181 | 2296 | `	int i = 0;` |
|    181 | 2297 | `	int bFirst = 1;` |
|    181 | 2298 | `	if( pbOpen ){` |
|    ! 0 | 2299 | `		*pbOpen = 0;` |
|    ! 0 | 2300 | `	}` |
|    181 | 2301 | `	SyBlobInit(&sField,&pVm->sAllocator);` |
|    151 | 2302 | `	for(;;){` |
|      - | 2303 | `		int bQuoted;` |
|    303 | 2304 | `		SyBlobReset(&sField);` |
|      - | 2305 | `		/* Whitespace in front of an OPENING enclosure is not part of the field --` |
|      - | 2306 | `		 * but only when an enclosure is what it leads to. */` |
|    303 | 2307 | `		if( i < nLimit ){` |
|    275 | 2308 | `			int t = i;` |
|    564 | 2309 | `			while( t < nLimit && (unsigned char)zInput[t] != delim` |
|    430 | 2310 | `			 && SyisSpace((unsigned char)zInput[t]) ){` |
|     19 | 2311 | `				t++;` |
|      1 | 2312 | `			}` |
|    275 | 2313 | `			if( t < nLimit && (unsigned char)zInput[t] == encl ){` |
|     89 | 2314 | `				i = t;` |
|     44 | 2315 | `			}` |
|    137 | 2316 | `		}` |
|    303 | 2317 | `		if( bFirst && i >= nLimit ){` |
|      - | 2318 | `			/* A record with no text at all is ONE null field, not an empty one. */` |
|     21 | 2319 | `			PH7_MemObjInit(pVm,&sEntry);` |
|     21 | 2320 | `			ph7_array_add_elem(pArray,0,&sEntry);` |
|     21 | 2321 | `			PH7_MemObjRelease(&sEntry);` |
|     21 | 2322 | `			break;` |
|      - | 2323 | `		}` |
|    283 | 2324 | `		bFirst = 0;` |
|    283 | 2325 | `		bQuoted = (i < nLimit && (unsigned char)zInput[i] == encl);` |
|    283 | 2326 | `		if( bQuoted ){` |
|     89 | 2327 | `			int bClosed = 0;` |
|     89 | 2328 | `			i++;` |
|    305 | 2329 | `			while( i < nLimit ){` |
|    301 | 2330 | `				int c = (unsigned char)zInput[i];` |
|      - | 2331 | `				/* The ENCLOSURE is tested first, which only shows when the two` |
|      - | 2332 | `` 				 * are the same character: `str_getcsv('"aa"b', ',', '"', '"')` `` |
|      - | 2333 | `				 * closes on the second quote rather than escaping past it. (The` |
|      - | 2334 | `				 * WRITER's order is the other way round -- php's is too.) */` |
|    301 | 2335 | `				if( c == encl ){` |
|     97 | 2336 | `					if( i + 1 < nLimit && (unsigned char)zInput[i+1] == encl ){` |
|      - | 2337 | `						/* Doubled: one literal enclosure. */` |
|     13 | 2338 | `						SyBlobAppend(&sField,(const void *)&zInput[i],sizeof(char));` |
|     13 | 2339 | `						i += 2;` |
|     13 | 2340 | `						continue;` |
|      - | 2341 | `					}` |
|     85 | 2342 | `					i++;` |
|     85 | 2343 | `					bClosed = 1;` |
|     85 | 2344 | `					break;` |
|      - | 2345 | `				}` |
|    205 | 2346 | `				if( escape != PH7_CSV_NO_ESCAPE && c == escape ){` |
|      - | 2347 | `					/* php keeps the escape AND the byte it protects. */` |
|      5 | 2348 | `					SyBlobAppend(&sField,(const void *)&zInput[i],sizeof(char));` |
|      5 | 2349 | `					i++;` |
|      5 | 2350 | `					if( i < nLimit ){` |
|      5 | 2351 | `						SyBlobAppend(&sField,(const void *)&zInput[i],sizeof(char));` |
|      5 | 2352 | `						i++;` |
|      2 | 2353 | `					}` |
|      5 | 2354 | `					continue;` |
|      - | 2355 | `				}` |
|    201 | 2356 | `				SyBlobAppend(&sField,(const void *)&zInput[i],sizeof(char));` |
|    201 | 2357 | `				i++;` |
|      1 | 2358 | `			}` |
|     89 | 2359 | `			if( !bClosed ){` |
|      - | 2360 | `				/* The text ran out with the enclosure still open, so the line` |
|      - | 2361 | `				 * ending stripped off the top is INSIDE the value: php puts it` |
|      - | 2362 | `				 * back (which is also how a record that continues on the next` |
|      - | 2363 | `				 * line keeps its embedded newline). */` |
|      5 | 2364 | `				if( nByte > nLimit ){` |
|      7 | 2365 | `					SyBlobAppend(&sField,(const void *)&zInput[nLimit],` |
|      4 | 2366 | `						(sxu32)(nByte - nLimit));` |
|      2 | 2367 | `				}` |
|      5 | 2368 | `				if( pbOpen ){` |
|    ! 0 | 2369 | `					*pbOpen = 1;` |
|    ! 0 | 2370 | `				}` |
|      2 | 2371 | `			}` |
|      - | 2372 | `			/* Whatever trails the closing enclosure belongs to the field too. */` |
|    121 | 2373 | `			while( i < nLimit && (unsigned char)zInput[i] != delim ){` |
|     33 | 2374 | `				SyBlobAppend(&sField,(const void *)&zInput[i],sizeof(char));` |
|     33 | 2375 | `				i++;` |
|      1 | 2376 | `			}` |
|     45 | 2377 | `		}else{` |
|    195 | 2378 | `			int iStart = i;` |
|      - | 2379 | `			int nRaw;` |
|    409 | 2380 | `			while( i < nLimit && (unsigned char)zInput[i] != delim ){` |
|    215 | 2381 | `				i++;` |
|      1 | 2382 | `			}` |
|    195 | 2383 | `			nRaw = CsvStripEol(&zInput[iStart],i - iStart);` |
|    195 | 2384 | `			if( nRaw > 0 ){` |
|    175 | 2385 | `				SyBlobAppend(&sField,(const void *)&zInput[iStart],(sxu32)nRaw);` |
|     87 | 2386 | `			}` |
|      - | 2387 | `		}` |
|    283 | 2388 | `		PH7_MemObjInitFromString(pVm,&sEntry,0);` |
|    283 | 2389 | `		if( SyBlobLength(&sField) > 0 ){` |
|    385 | 2390 | `			ph7_value_string(&sEntry,(const char *)SyBlobData(&sField),` |
|    256 | 2391 | `				(int)SyBlobLength(&sField));` |
|    128 | 2392 | `		}` |
|    283 | 2393 | `		ph7_array_add_elem(pArray,0,&sEntry);` |
|    283 | 2394 | `		PH7_MemObjRelease(&sEntry);` |
|    283 | 2395 | `		if( i < nLimit && (unsigned char)zInput[i] == delim ){` |
|      - | 2396 | `			/* A trailing delimiter still opens one more (empty) field. */` |
|    123 | 2397 | `			i++;` |
|    123 | 2398 | `			continue;` |
|      - | 2399 | `		}` |
|    161 | 2400 | `		break;` |
|    ! 0 | 2401 | `	}` |
|    181 | 2402 | `	SyBlobRelease(&sField);` |
|    181 | 2403 | `	return SXRET_OK;` |
|      1 | 2404 | `}` |
|      - | 2405 | `/*` |
|      - | 2406 | ` * Validate a CSV $separator/$enclosure/$escape argument like php 8 and` |
|      - | 2407 | ` * extract its character. $separator/$enclosure must be exactly one` |
|      - | 2408 | ` * character; $escape may also be empty, which disables escape processing` |
|      - | 2409 | ` * (the caller gets PH7_CSV_NO_ESCAPE). The argument is coerced to string` |
|      - | 2410 | ` * first like php's ZPP, so an int 5 separates on "5"; null and array` |
|      - | 2411 | ` * arguments never reach here — the central type screen rejects them.` |
|      - | 2412 | ` * Returns PH7_OK on success; otherwise the ValueError has been thrown and` |
|      - | 2413 | ` * the caller must return the propagated status.` |
|      - | 2414 | ` */` |
|    866 | 2415 | `PH7_PRIVATE sxi32 PH7_CsvCharArg(ph7_context *pCtx,ph7_value *pArg,int iArg,` |
|      - | 2416 | `	const char *zName,int bAllowEmpty,int *pChar)` |
|      1 | 2417 | `{` |
|      - | 2418 | `	const char *zPtr;` |
|      - | 2419 | `	int n;` |
|    867 | 2420 | `	zPtr = ph7_value_to_string(pArg,&n);` |
|    867 | 2421 | `	if( n == 1 ){` |
|      - | 2422 | `		/* UNSIGNED: every comparison downstream is against a byte read out of a` |
|      - | 2423 | `		 * field, so a separator of "\xE9" stored as a negative char would match` |
|      - | 2424 | `		 * nothing at all and the field would go out unquoted. */` |
|    813 | 2425 | `		*pChar = (unsigned char)zPtr[0];` |
|    813 | 2426 | `		return PH7_OK;` |
|      - | 2427 | `	}` |
|     55 | 2428 | `	if( n < 1 && bAllowEmpty ){` |
|     17 | 2429 | `		*pChar = PH7_CSV_NO_ESCAPE;` |
|     17 | 2430 | `		return PH7_OK;` |
|      - | 2431 | `	}` |
|     58 | 2432 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2433 | `		"%s(): Argument #%d ($%s) must be %sa single character",` |
|     19 | 2434 | `		ph7_function_name(pCtx),iArg,zName,bAllowEmpty ? "empty or " : ""` |
|      - | 2435 | `		);` |
|    434 | 2436 | `}` |
|      - | 2437 | `/*` |
|      - | 2438 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 2439 | ` *  Parse a CSV string into an array.` |
|      - | 2440 | ` * Parameters` |
|      - | 2441 | ` *  $input` |
|      - | 2442 | ` *   The string to parse.` |
|      - | 2443 | ` *  $delimiter` |
|      - | 2444 | ` *   Set the field delimiter (one character only).` |
|      - | 2445 | ` *  $enclosure` |
|      - | 2446 | ` *   Set the field enclosure character (one character only).` |
|      - | 2447 | ` *  $escape` |
|      - | 2448 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 2449 | ` * Return` |
|      - | 2450 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 2451 | ` */` |
|    120 | 2452 | `PH7_PRIVATE int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2453 | `{` |
|      - | 2454 | `	const char *zInput;` |
|      - | 2455 | `	ph7_value *pArray;` |
|    121 | 2456 | `	int delim  = ',';   /* Delimiter */` |
|    121 | 2457 | `	int encl   = '"' ;  /* Enclosure */` |
|    121 | 2458 | `	int escape = '\\';  /* Escape character */` |
|      - | 2459 | `	int nLen;` |
|    121 | 2460 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2461 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 2462 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2463 | `		return PH7_OK;` |
|      - | 2464 | `	}` |
|      - | 2465 | `	/* Extract the raw input */` |
|    121 | 2466 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|    121 | 2467 | `	if( nArg > 1 ){` |
|    119 | 2468 | `		sxi32 rc = PH7_CsvCharArg(pCtx,apArg[1],2,"separator",0,&delim);` |
|    119 | 2469 | `		if( rc != PH7_OK ){` |
|      5 | 2470 | `			return rc;` |
|      - | 2471 | `		}` |
|    115 | 2472 | `		if( nArg > 2 ){` |
|    115 | 2473 | `			rc = PH7_CsvCharArg(pCtx,apArg[2],3,"enclosure",0,&encl);` |
|    115 | 2474 | `			if( rc != PH7_OK ){` |
|      5 | 2475 | `				return rc;` |
|      - | 2476 | `			}` |
|    111 | 2477 | `			if( nArg > 3 ){` |
|    111 | 2478 | `				rc = PH7_CsvCharArg(pCtx,apArg[3],4,"escape",1,&escape);` |
|    111 | 2479 | `				if( rc != PH7_OK ){` |
|      3 | 2480 | `					return rc;` |
|      - | 2481 | `				}` |
|     54 | 2482 | `			}` |
|     54 | 2483 | `		}` |
|     54 | 2484 | `	}` |
|      - | 2485 | `	/* Create our array */` |
|    111 | 2486 | `	pArray = ph7_context_new_array(pCtx);` |
|    111 | 2487 | `	if( pArray == 0 ){` |
|      - | 2488 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 2489 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2490 | `	}` |
|      - | 2491 | `	/* Parse the raw input */` |
|    111 | 2492 | `	PH7_ProcessCsv(pArray,zInput,nLen,delim,encl,escape,0);` |
|      - | 2493 | `	/* Return the freshly created array */` |
|    111 | 2494 | `	ph7_result_value(pCtx,pArray);` |
|    111 | 2495 | `	return PH7_OK;` |
|     61 | 2496 | `}` |
|      - | 2497 | `/*` |
|      - | 2498 | `` * Is the collected tag (`<a href=...>`, `</a>`) one of the allowed ones?`` |
|      - | 2499 | ` *` |
|      - | 2500 | ` * php normalizes what it collected — lowercased, leading/trailing whitespace` |
|      - | 2501 | `` * dropped, everything after the tag NAME dropped, a closing `</x>` read as`` |
|      - | 2502 | `` * `<x>` — and then looks for that `<name>` as a SUBSTRING of the allow string,`` |
|      - | 2503 | `` * which is why the allow list is written as `"<a><b>"` and why a tag name that`` |
|      - | 2504 | ` * is a prefix of an allowed one is refused.` |
|      - | 2505 | ` */` |
|     66 | 2506 | `static int FvTagAllowed(const char *zTag,int nTag,const char *zAllow,int nAllow)` |
|      2 | 2507 | `{` |
|      - | 2508 | `	char zNorm[128];` |
|     68 | 2509 | `	int n = 0, i = 0, state = 0, done = 0;` |
|     68 | 2510 | `	if( nTag<1 \|\| nAllow<1 ){ return 0; }` |
|    338 | 2511 | `	while( !done && i<=nTag && n<(int)sizeof(zNorm)-2 ){` |
|      - | 2512 | `		/* php walks past the collected span into its NUL terminator */` |
|    272 | 2513 | `		int c = (i<nTag) ? SyToLower((unsigned char)zTag[i]) : 0;` |
|    272 | 2514 | `		switch( c ){` |
|     33 | 2515 | `		case '<':` |
|     68 | 2516 | `			zNorm[n++] = (char)c;` |
|     68 | 2517 | `			break;` |
|     32 | 2518 | `		case '>':` |
|     66 | 2519 | `			done = 1;` |
|     66 | 2520 | `			break;` |
|     70 | 2521 | `		default:` |
|    142 | 2522 | `			if( c && !SyisSpace((unsigned char)c) ){` |
|    140 | 2523 | `				if( state==0 ){ state = 1; }` |
|    140 | 2524 | `				if( c!='/' \|\| (i>0 && zTag[i-1]!='<' && !(i+1<nTag && zTag[i+1]=='>')) ){` |
|    106 | 2525 | `					zNorm[n++] = (char)c;` |
|     52 | 2526 | `				}` |
|     71 | 2527 | `			}else{` |
|      3 | 2528 | `				if( state==1 ){ done = 1; }` |
|      3 | 2529 | `				if( c==0 ){ done = 1; }` |
|      - | 2530 | `			}` |
|    140 | 2531 | `			break;` |
|      - | 2532 | `		}` |
|    272 | 2533 | `		i++;` |
|      2 | 2534 | `	}` |
|     68 | 2535 | `	zNorm[n++] = '>';` |
|     68 | 2536 | `	zNorm[n] = 0;` |
|      - | 2537 | `	/* the allow list is matched case-insensitively as a substring */` |
|     96 | 2538 | `	for( i=0; i+n<=nAllow; i++ ){` |
|      - | 2539 | `		int j;` |
|    244 | 2540 | `		for( j=0; j<n; j++ ){` |
|    198 | 2541 | `			if( SyToLower((unsigned char)zAllow[i+j]) != (unsigned char)zNorm[j] ){ break; }` |
|     86 | 2542 | `		}` |
|     76 | 2543 | `		if( j==n ){ return 1; }` |
|     16 | 2544 | `	}` |
|     22 | 2545 | `	return 0;` |
|     35 | 2546 | `}` |
|      - | 2547 | `/*` |
|      - | 2548 | ` * strip_tags(), a port of php's own state machine.` |
|      - | 2549 | ` *` |
|      - | 2550 | ` * State 0 is the output, state 1 an html tag, state 2 a php tag, state 3 an` |
|      - | 2551 | `` * `<!` construct and state 4 a comment. What the previous hand-rolled scan (find`` |
|      - | 2552 | ` * a '<', drop through to the next '>') got wrong is everything the machine` |
|      - | 2553 | ` * tracks beside the two brackets: a '<' followed by WHITESPACE is text and not a` |
|      - | 2554 | ` * tag opener; a quoted attribute may hold a '>' without closing the tag; a` |
|      - | 2555 | ` * nested '<' inside a tag raises a depth that the matching '>' lowers; a NUL is` |
|      - | 2556 | `` * dropped rather than ending the scan; `<?php ... ?>`, `<!-- ... -->` and`` |
|      - | 2557 | `` * `<!DOCTYPE ...>` each have their own exit rule; and an unterminated tag eats`` |
|      - | 2558 | ` * the rest of the input instead of reappearing as text.` |
|      - | 2559 | ` */` |
|     88 | 2560 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen,int bTagSpaces)` |
|      4 | 2561 | `{` |
|      - | 2562 | `	SyBlob sOut, sTag;` |
|     92 | 2563 | `	int i = 0, state = 0, depth = 0, in_q = 0, br = 0, is_xml = 0;` |
|     92 | 2564 | `	int lc = 0;` |
|     92 | 2565 | `	int bAllow = (zTaglist && nTaglen>0);` |
|     92 | 2566 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     92 | 2567 | `	SyBlobInit(&sTag,&pCtx->pVm->sAllocator);` |
|   1310 | 2568 | `	while( i<nByte ){` |
|   1221 | 2569 | `		unsigned char c = (unsigned char)zIn[i];` |
|      - | 2570 | `		/* php reads one byte past the current one; its buffer is NUL-terminated */` |
|   1221 | 2571 | `		unsigned char nx = (i+1<nByte) ? (unsigned char)zIn[i+1] : 0;` |
|   1221 | 2572 | `		switch( state ){` |
|    297 | 2573 | `		case 0:` |
|    597 | 2574 | `			if( c==0 ){` |
|      3 | 2575 | `				break;` |
|    595 | 2576 | `			}else if( c=='<' && !in_q ){` |
|    161 | 2577 | `				if( SyisSpace(nx) && !bTagSpaces ){` |
|      3 | 2578 | `					SyBlobAppend(&sOut,&zIn[i],1);` |
|      3 | 2579 | `					break;` |
|      - | 2580 | `				}` |
|    159 | 2581 | `				lc = '<';` |
|    159 | 2582 | `				state = 1;` |
|    159 | 2583 | `				if( bAllow ){ SyBlobAppend(&sTag,"<",1); }` |
|    159 | 2584 | `				i++;` |
|    159 | 2585 | `				continue;` |
|    437 | 2586 | `			}else if( c=='>' ){` |
|    ! 0 | 2587 | `				if( depth ){ depth--; break; }` |
|    ! 0 | 2588 | `				if( in_q ){ break; }` |
|    ! 0 | 2589 | `				SyBlobAppend(&sOut,&zIn[i],1);` |
|    ! 0 | 2590 | `			}else{` |
|    437 | 2591 | `				SyBlobAppend(&sOut,&zIn[i],1);` |
|      - | 2592 | `			}` |
|    437 | 2593 | `			break;` |
|    258 | 2594 | `		case 1:` |
|    519 | 2595 | `			if( c==0 ){` |
|      3 | 2596 | `				break;` |
|    517 | 2597 | `			}else if( c=='<' && !in_q ){` |
|      5 | 2598 | `				if( SyisSpace(nx) && !bTagSpaces ){` |
|    ! 0 | 2599 | `					if( bAllow ){ SyBlobAppend(&sTag,&zIn[i],1); }` |
|    ! 0 | 2600 | `					break;` |
|      - | 2601 | `				}` |
|      5 | 2602 | `				depth++;` |
|      5 | 2603 | `				break;` |
|    513 | 2604 | `			}else if( c=='>' ){` |
|    155 | 2605 | `				if( depth ){ depth--; break; }` |
|    151 | 2606 | `				if( in_q ){ break; }` |
|    147 | 2607 | `				lc = '>';` |
|    147 | 2608 | `				if( is_xml && i>=1 && zIn[i-1]=='-' ){ break; }` |
|    147 | 2609 | `				in_q = state = is_xml = 0;` |
|    147 | 2610 | `				if( bAllow ){` |
|     68 | 2611 | `					SyBlobAppend(&sTag,">",1);` |
|    101 | 2612 | `					if( FvTagAllowed((const char *)SyBlobData(&sTag),(int)SyBlobLength(&sTag),` |
|     33 | 2613 | `					                 zTaglist,nTaglen) ){` |
|     48 | 2614 | `						SyBlobAppend(&sOut,SyBlobData(&sTag),SyBlobLength(&sTag));` |
|     23 | 2615 | `					}` |
|     68 | 2616 | `					SyBlobReset(&sTag);` |
|     33 | 2617 | `				}` |
|    147 | 2618 | `				i++;` |
|    147 | 2619 | `				continue;` |
|    361 | 2620 | `			}else if( c=='"' \|\| c=='\'' ){` |
|     13 | 2621 | `				if( i!=0 && (!in_q \|\| (int)c==in_q) ){` |
|     13 | 2622 | `					in_q = in_q ? 0 : (int)c;` |
|      6 | 2623 | `				}` |
|     13 | 2624 | `				if( bAllow ){ SyBlobAppend(&sTag,&zIn[i],1); }` |
|    355 | 2625 | `			}else if( c=='!' && i>=1 && zIn[i-1]=='<' ){` |
|      5 | 2626 | `				state = 3;` |
|      5 | 2627 | `				lc = c;` |
|      5 | 2628 | `				i++;` |
|      5 | 2629 | `				continue;` |
|    345 | 2630 | `			}else if( c=='?' && i>=1 && zIn[i-1]=='<' ){` |
|      5 | 2631 | `				br = 0;` |
|      5 | 2632 | `				state = 2;` |
|      5 | 2633 | `				i++;` |
|      5 | 2634 | `				continue;` |
|    ! 0 | 2635 | `			}else{` |
|    341 | 2636 | `				if( bAllow ){ SyBlobAppend(&sTag,&zIn[i],1); }` |
|      - | 2637 | `			}` |
|    353 | 2638 | `			break;` |
|     37 | 2639 | `		case 2:` |
|     75 | 2640 | `			if( c=='(' ){` |
|    ! 0 | 2641 | `				if( lc!='"' && lc!='\'' ){ lc = '('; br++; }` |
|     75 | 2642 | `			}else if( c==')' ){` |
|    ! 0 | 2643 | `				if( lc!='"' && lc!='\'' ){ lc = ')'; br--; }` |
|     75 | 2644 | `			}else if( c=='>' ){` |
|      7 | 2645 | `				if( depth ){ depth--; break; }` |
|      7 | 2646 | `				if( in_q ){ break; }` |
|      5 | 2647 | `				if( !br && i>=1 && lc!='"' && zIn[i-1]=='?' ){` |
|      5 | 2648 | `					in_q = state = 0;` |
|      5 | 2649 | `					SyBlobReset(&sTag);` |
|      5 | 2650 | `					i++;` |
|      5 | 2651 | `					continue;` |
|    ! 0 | 2652 | `				}` |
|     69 | 2653 | `			}else if( c=='"' \|\| c=='\'' ){` |
|      9 | 2654 | `				if( i>=1 && zIn[i-1]!='\\' ){` |
|      9 | 2655 | `					if( lc==(int)c ){ lc = 0; }` |
|      5 | 2656 | `					else if( lc!='\\' ){ lc = (int)c; }` |
|      9 | 2657 | `					if( i!=0 && (!in_q \|\| (int)c==in_q) ){` |
|      9 | 2658 | `						in_q = in_q ? 0 : (int)c;` |
|      4 | 2659 | `					}` |
|      5 | 2660 | `				}` |
|     65 | 2661 | `			}else if( c=='l' \|\| c=='L' ){` |
|      - | 2662 | ``				/* `<?xml` is not php: back to the html state */`` |
|      2 | 2663 | `				if( state==2 && i>4` |
|      1 | 2664 | `				 && (zIn[i-1]=='m' \|\| zIn[i-1]=='M')` |
|    ! 0 | 2665 | `				 && (zIn[i-2]=='x' \|\| zIn[i-2]=='X')` |
|      1 | 2666 | `				 && zIn[i-3]=='?' && zIn[i-4]=='<' ){` |
|    ! 0 | 2667 | `					state = 1; is_xml = 1;` |
|    ! 0 | 2668 | `					i++;` |
|    ! 0 | 2669 | `					continue;` |
|      - | 2670 | `				}` |
|      1 | 2671 | `			}` |
|     69 | 2672 | `			break;` |
|      9 | 2673 | `		case 3:` |
|     19 | 2674 | `			if( c=='>' ){` |
|    ! 0 | 2675 | `				if( depth ){ depth--; break; }` |
|    ! 0 | 2676 | `				if( in_q ){ break; }` |
|    ! 0 | 2677 | `				in_q = state = 0;` |
|    ! 0 | 2678 | `				SyBlobReset(&sTag);` |
|    ! 0 | 2679 | `				i++;` |
|    ! 0 | 2680 | `				continue;` |
|     19 | 2681 | `			}else if( c=='"' \|\| c=='\'' ){` |
|    ! 0 | 2682 | `				if( i!=0 && zIn[i-1]!='\\' && (!in_q \|\| (int)c==in_q) ){` |
|    ! 0 | 2683 | `					in_q = in_q ? 0 : (int)c;` |
|    ! 0 | 2684 | `				}` |
|     19 | 2685 | `			}else if( c=='-' ){` |
|      5 | 2686 | `				if( i>=2 && zIn[i-1]=='-' && zIn[i-2]=='!' ){` |
|      3 | 2687 | `					state = 4;` |
|      3 | 2688 | `					i++;` |
|      3 | 2689 | `					continue;` |
|      1 | 2690 | `				}` |
|     16 | 2691 | `			}else if( c=='E' \|\| c=='e' ){` |
|      - | 2692 | `				/* the !DOCTYPE exception */` |
|      2 | 2693 | `				if( i>6` |
|      2 | 2694 | `				 && (zIn[i-1]=='p'\|\|zIn[i-1]=='P') && (zIn[i-2]=='y'\|\|zIn[i-2]=='Y')` |
|      2 | 2695 | `				 && (zIn[i-3]=='t'\|\|zIn[i-3]=='T') && (zIn[i-4]=='c'\|\|zIn[i-4]=='C')` |
|      3 | 2696 | `				 && (zIn[i-5]=='o'\|\|zIn[i-5]=='O') && (zIn[i-6]=='d'\|\|zIn[i-6]=='D') ){` |
|      3 | 2697 | `					state = 1;` |
|      3 | 2698 | `					i++;` |
|      3 | 2699 | `					continue;` |
|      - | 2700 | `				}` |
|    ! 0 | 2701 | `			}` |
|     15 | 2702 | `			break;` |
|      8 | 2703 | `		default: /* state 4: inside a comment */` |
|     17 | 2704 | `			if( c=='>' && !in_q && i>=2 && zIn[i-1]=='-' && zIn[i-2]=='-' ){` |
|      3 | 2705 | `				in_q = state = 0;` |
|      3 | 2706 | `				SyBlobReset(&sTag);` |
|      1 | 2707 | `			}` |
|     16 | 2708 | `			break;` |
|      - | 2709 | `		}` |
|    905 | 2710 | `		i++;` |
|      3 | 2711 | `	}` |
|     92 | 2712 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     92 | 2713 | `	SyBlobRelease(&sOut);` |
|     92 | 2714 | `	SyBlobRelease(&sTag);` |
|     92 | 2715 | `	return SXRET_OK;` |
|      4 | 2716 | `}` |
|      - | 2717 | `/*` |
|      - | 2718 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 2719 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 2720 | ` * Parameters` |
|      - | 2721 | ` *  $str` |
|      - | 2722 | ` *  The input string.` |
|      - | 2723 | ` * $allowable_tags` |
|      - | 2724 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 2725 | ` * Return` |
|      - | 2726 | ` *  Returns the stripped string.` |
|      - | 2727 | ` */` |
|     14 | 2728 | `static int VmStripTagsListWalk(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 2729 | `{` |
|     15 | 2730 | `	SyBlob *pOut = (SyBlob *)pUserData;` |
|      - | 2731 | `	ph7_value sTmp;` |
|      - | 2732 | `	const char *zTag; int nTag;` |
|      7 | 2733 | `	SXUNUSED(pKey);` |
|      - | 2734 | `	/* through a COPY: the array is the caller's */` |
|     15 | 2735 | `	PH7_MemObjInit(pData->pVm,&sTmp);` |
|     15 | 2736 | `	zTag = ph7_value_to_string(PH7_ValuePeek(pData,&sTmp),&nTag);` |
|     15 | 2737 | `	SyBlobAppend(pOut,"<",1);` |
|     15 | 2738 | `	if( nTag>0 ){ SyBlobAppend(pOut,zTag,(sxu32)nTag); }` |
|     15 | 2739 | `	SyBlobAppend(pOut,">",1);` |
|     15 | 2740 | `	PH7_MemObjRelease(&sTmp);` |
|     15 | 2741 | `	return PH7_OK;` |
|      1 | 2742 | `}` |
|     62 | 2743 | `PH7_PRIVATE int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2744 | `{` |
|     65 | 2745 | `	const char *zTaglist = 0;` |
|      - | 2746 | `	const char *zString;` |
|      - | 2747 | `	SyBlob sTags;` |
|     65 | 2748 | `	int nTaglen = 0;` |
|      - | 2749 | `	int nLen;` |
|     65 | 2750 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2751 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 2752 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2753 | `		return PH7_OK;` |
|      - | 2754 | `	}` |
|      - | 2755 | `	/* Point to the raw string */` |
|     65 | 2756 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     65 | 2757 | `	SyBlobInit(&sTags,&pCtx->pVm->sAllocator);` |
|     65 | 2758 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 2759 | `		/* Allowed tag */` |
|     14 | 2760 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|     59 | 2761 | `	}else if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|      - | 2762 | `		/* php 7.4's ARRAY spelling of the same list: each entry is a bare tag` |
|      - | 2763 | ``		 * NAME, and php builds the `<a><b>` string out of them. */`` |
|     15 | 2764 | `		ph7_array_walk(apArg[1],VmStripTagsListWalk,&sTags);` |
|     15 | 2765 | `		zTaglist = (const char *)SyBlobData(&sTags);` |
|     15 | 2766 | `		nTaglen = (int)SyBlobLength(&sTags);` |
|      7 | 2767 | `	}` |
|      - | 2768 | `	/* Process input */` |
|     65 | 2769 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen,0);` |
|     65 | 2770 | `	SyBlobRelease(&sTags);` |
|     65 | 2771 | `	return PH7_OK;` |
|     34 | 2772 | `}` |
|      - | 2773 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 2774 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 2775 | `/*` |
|      - | 2776 | ` * Parse an INI string.` |
|      - | 2777 |  |
|      - | 2778 | ` * According to wikipedia` |
|      - | 2779 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 2780 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 2781 | ` *  Format` |
|      - | 2782 | `*    Properties` |
|      - | 2783 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 2784 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 2785 | `*     Example:` |
|      - | 2786 | `*      name=value` |
|      - | 2787 | `*    Sections` |
|      - | 2788 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 2789 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 2790 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 2791 | `*     or the end of the file. Sections may not be nested.` |
|      - | 2792 | `*     Example:` |
|      - | 2793 | `*      [section]` |
|      - | 2794 | `*   Comments` |
|      - | 2795 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 2796 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 2797 | `*/` |
|      - | 2798 | `/*` |
|      - | 2799 | ` * The ini scanner's ${NAME} expansion, php's zend_ini_get_var(): a known ini` |
|      - | 2800 | ` * OPTION answers first and the process environment second, then the` |
|      - | 2801 | `` * `${NAME:-fallback}` text, then the empty string — a defined CONSTANT`` |
|      - | 2802 | ` * deliberately does NOT answer here (that is the bare-identifier rule below).` |
|      - | 2803 | ` * Only an UNSET name reaches the fallback: one set to the EMPTY string answers` |
|      - | 2804 | ` * that empty string, which is why the environment lookup is judged by its` |
|      - | 2805 | ` * return code and not by what it wrote. The VFS environment reader answers` |
|      - | 2806 | ` * through the call context's RESULT slot (it was written for getenv()), so the` |
|      - | 2807 | ` * read borrows pCtx->pRet around the call and empties it again; the parse's own` |
|      - | 2808 | ` * result is not written until the very end.` |
|      - | 2809 | ` */` |
|     34 | 2810 | `static void VmIniExpandDollarVar(ph7_context *pCtx,const char *zName,sxu32 nName,SyBlob *pOut,` |
|      - | 2811 | `	const char *zFall,sxu32 nFall)` |
|      2 | 2812 | `{` |
|      - | 2813 | `	char zVar[128];` |
|      - | 2814 | `	SyBlob sVal;` |
|     36 | 2815 | `	if( nName >= sizeof(zVar) ){` |
|    ! 0 | 2816 | `		return; /* php answers "" for an unknown name; an unreasonable one is unknown */` |
|      - | 2817 | `	}` |
|     36 | 2818 | `	if( nName > 0 ){` |
|     36 | 2819 | `		SyMemcpy(zName,zVar,nName);` |
|     17 | 2820 | `	}` |
|     36 | 2821 | `	zVar[nName] = 0;` |
|     36 | 2822 | `	if( nName > 0 ){` |
|     36 | 2823 | `		SyBlobInit(&sVal,&pCtx->pVm->sAllocator);` |
|     36 | 2824 | `		PH7_VmIniGetStr(pCtx->pVm,zVar,&sVal);` |
|     36 | 2825 | `		if( SyBlobLength(&sVal) > 0 ){` |
|    ! 0 | 2826 | `			SyBlobAppend(pOut,SyBlobData(&sVal),SyBlobLength(&sVal));` |
|    ! 0 | 2827 | `			SyBlobRelease(&sVal);` |
|    ! 0 | 2828 | `			return;` |
|      - | 2829 | `		}` |
|     36 | 2830 | `		SyBlobRelease(&sVal);` |
|      - | 2831 | `		{` |
|     36 | 2832 | `			const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     36 | 2833 | `			ph7_value *pRet = pCtx->pRet;` |
|     36 | 2834 | `			sxu32 nBefore = SyBlobLength(&pRet->sBlob);` |
|     36 | 2835 | `			if( pVfs && pVfs->xGetenv ){` |
|     36 | 2836 | `				if( pVfs->xGetenv(zVar,pCtx) == PH7_OK ){` |
|     12 | 2837 | `					if( SyBlobLength(&pRet->sBlob) > nBefore ){` |
|     14 | 2838 | `						SyBlobAppend(pOut,(const char *)SyBlobData(&pRet->sBlob) + nBefore,` |
|      8 | 2839 | `							SyBlobLength(&pRet->sBlob) - nBefore);` |
|      4 | 2840 | `					}` |
|     12 | 2841 | `					ph7_value_reset_string_cursor(pRet);` |
|     12 | 2842 | `					return;` |
|      - | 2843 | `				}` |
|     26 | 2844 | `				ph7_value_reset_string_cursor(pRet);` |
|     12 | 2845 | `			}` |
|      - | 2846 | `		}` |
|     12 | 2847 | `	}` |
|     26 | 2848 | `	if( nFall > 0 ){` |
|     21 | 2849 | `		SyBlobAppend(pOut,zFall,nFall);` |
|     10 | 2850 | `	}` |
|     19 | 2851 | `}` |
|      - | 2852 | `/*` |
|      - | 2853 | ` * php's ini VALUE grammar, derived from Zend/zend_ini_scanner.l and` |
|      - | 2854 | ` * Zend/zend_ini_parser.y and re-derived against the running php.` |
|      - | 2855 | ` *` |
|      - | 2856 | ` * An unquoted value is NOT one literal token. The scanner cuts it up -- a` |
|      - | 2857 | ``  * maximal run of value bytes is ONE token, and `&`, `\|`, `^`, `~`, `!`, `(` `` |
|      - | 2858 | `` * and `)` are operators of their own -- and the parser either CONCATENATES`` |
|      - | 2859 | ` * the pieces, when no operator appears, or evaluates them as a 32-bit` |
|      - | 2860 | ` * bitwise expression:` |
|      - | 2861 | ` *` |
|      - | 2862 | `` *   . `foo bar`           -> "foo bar": two pieces joined, each looked up as`` |
|      - | 2863 | ` *                           a constant and left standing as its own name` |
|      - | 2864 | ` *                           when none is defined.` |
|      - | 2865 | `` *   . `E_ALL & ~E_NOTICE` -> "30711": every operand goes through atoi(), the`` |
|      - | 2866 | `` *                           whole thing is computed in an `int`, and the`` |
|      - | 2867 | ` *                           DECIMAL TEXT of that int is what gets stored.` |
|      - | 2868 | `` *   . `foo\|bar`           -> "0": the same rule, and neither name reads as a`` |
|      - | 2869 | ` *                           number.` |
|      - | 2870 | `` *   . `something (note)`  -> a syntax error -- and one syntax error anywhere`` |
|      - | 2871 | ` *                           makes the WHOLE parse answer FALSE.` |
|      - | 2872 | ` *` |
|      - | 2873 | `` * A run is one token, so `MYC.x` is those five bytes and never the constant:`` |
|      - | 2874 | ` * the constant lookup fires only when the run is exactly an identifier, and` |
|      - | 2875 | ` * the numeric shape INI_SCANNER_TYPED converts fires only when the run is` |
|      - | 2876 | `` * exactly a number (`1e3` is a string, `007` is the int 7).`` |
|      - | 2877 | ` *` |
|      - | 2878 | ` * The boolean words are tokens of their own, so they may stand as the whole` |
|      - | 2879 | `` * value and nowhere else: `on x` and `x on` are both syntax errors.`` |
|      - | 2880 | ` */` |
|      - | 2881 | `#define VM_INI_VAL_MAX_DEPTH 32` |
|      - | 2882 | `typedef struct VmIniVal VmIniVal;` |
|      - | 2883 | `struct VmIniVal {` |
|      - | 2884 | `	ph7_context *pCtx;` |
|      - | 2885 | `	const char *zCur;` |
|      - | 2886 | `	const char *zEnd;` |
|      - | 2887 | `	const char *zErr;    /* first byte of the offending token, 0 while all is well */` |
|      - | 2888 | `	const char *zTok;    /* how php's parser names that token in its warning */` |
|      - | 2889 | ``	char zOp[32];        /* room for the `'X'` an operator is named by, and for`` |
|      - | 2890 | ``	                      * the `, expecting '='` an offset statement adds */`` |
|      - | 2891 | `};` |
|      - | 2892 | `/*` |
|      - | 2893 | ` * One parsed piece: the text php would store in INI_SCANNER_NORMAL, plus the` |
|      - | 2894 | ` * two marks INI_SCANNER_TYPED needs -- whether an operator ran (the value is` |
|      - | 2895 | ` * that int) and whether the leading token was a NUMBER (php's parser carries` |
|      - | 2896 | `` * that mark through a concatenation and through parentheses, so `((1))` and`` |
|      - | 2897 | `` * `1 ` are ints while `(E_ALL)` and `1 2` are strings).`` |
|      - | 2898 | ` */` |
|      - | 2899 | `typedef struct VmIniRes VmIniRes;` |
|      - | 2900 | `struct VmIniRes {` |
|      - | 2901 | `	SyBlob sText;` |
|      - | 2902 | `	sxi32 iNum;` |
|      - | 2903 | `	int bOp;` |
|      - | 2904 | `	int bNumTok;` |
|      - | 2905 | `};` |
|      - | 2906 | `static const struct {` |
|      - | 2907 | `	const char *zWord;` |
|      - | 2908 | `	sxu32 nWord;` |
|      - | 2909 | `	const char *zTok;` |
|      - | 2910 | `	int iKind;           /* 1: true, 0: false, 2: null */` |
|      - | 2911 | `} aIniBool[] = {` |
|      - | 2912 | `	{ "true" ,4,"BOOL_TRUE" ,1 }, { "on"  ,2,"BOOL_TRUE" ,1 }, { "yes",3,"BOOL_TRUE",1 },` |
|      - | 2913 | `	{ "false",5,"BOOL_FALSE",0 }, { "off" ,3,"BOOL_FALSE",0 }, { "no" ,2,"BOOL_FALSE",0 },` |
|      - | 2914 | `	{ "none" ,4,"BOOL_FALSE",0 }, { "null",4,"NULL_NULL" ,2 }` |
|      - | 2915 | `};` |
|   3472 | 2916 | `static int VmIniValIsBinOp(int c)` |
|      4 | 2917 | `{` |
|   3476 | 2918 | `	return c == '\|' \|\| c == '&' \|\| c == '^';` |
|      4 | 2919 | `}` |
|   3332 | 2920 | `static int VmIniValIsOperator(int c)` |
|      4 | 2921 | `{` |
|   3336 | 2922 | `	return VmIniValIsBinOp(c) \|\| c == '~' \|\| c == '!' \|\| c == '(' \|\| c == ')';` |
|      4 | 2923 | `}` |
|      - | 2924 | `/*` |
|      - | 2925 | `` * php's VALUE_CHARS: every byte a run may hold. A `$` joins the run only when`` |
|      - | 2926 | `` * a byte that is not `{` follows it (php's LITERAL_DOLLAR), so `$x` is text`` |
|      - | 2927 | `` * and `${x}` is a variable.`` |
|      - | 2928 | ` */` |
|   3530 | 2929 | `static int VmIniValRunChar(const char *z,const char *zEnd,int *pnLen)` |
|      4 | 2930 | `{` |
|   3534 | 2931 | `	int c = (unsigned char)z[0];` |
|   3534 | 2932 | `	if( c == '$' ){` |
|     38 | 2933 | `		if( &z[1] < zEnd && z[1] != '{' ){` |
|    ! 0 | 2934 | `			*pnLen = (z[1] == '\\' && &z[2] < zEnd) ? 3 : 2;` |
|    ! 0 | 2935 | `			return 1;` |
|      - | 2936 | `		}` |
|     38 | 2937 | `		return 0;` |
|      - | 2938 | `	}` |
|   3494 | 2939 | `	if( c == '=' \|\| c == ' ' \|\| c == '\t' \|\| c == '\n' \|\| c == '\r'` |
|   3324 | 2940 | `	 \|\| c == ';' \|\| c == '"' \|\| c == '\'' \|\| VmIniValIsOperator(c) ){` |
|    575 | 2941 | `		return 0;` |
|      - | 2942 | `	}` |
|   2926 | 2943 | `	*pnLen = 1;` |
|   2926 | 2944 | `	return 1;` |
|   1769 | 2945 | `}` |
|   1078 | 2946 | `static int VmIniValBoolWord(const char *z,sxu32 n)` |
|      4 | 2947 | `{` |
|      - | 2948 | `	sxu32 i;` |
|   9392 | 2949 | `	for( i = 0 ; i < SX_ARRAYSIZE(aIniBool) ; i++ ){` |
|   8380 | 2950 | `		if( n == aIniBool[i].nWord && SyStrnicmp(z,aIniBool[i].zWord,n) == 0 ){` |
|     68 | 2951 | `			return (int)i;` |
|      - | 2952 | `		}` |
|   4159 | 2953 | `	}` |
|   1016 | 2954 | `	return -1;` |
|    543 | 2955 | `}` |
|      - | 2956 | ``/* php's NUMBER: `-?[0-9]+`, or a decimal with digits on at least one side and`` |
|      - | 2957 | `` * no sign at all (`-0.5` is therefore a string, not a number). */`` |
|    618 | 2958 | `static int VmIniValRunIsNumber(const char *z,sxu32 n)` |
|      4 | 2959 | `{` |
|    622 | 2960 | `	sxu32 i = 0,nDig = 0,nDot = 0;` |
|    622 | 2961 | `	if( n < 1 ){` |
|    ! 0 | 2962 | `		return 0;` |
|      - | 2963 | `	}` |
|    622 | 2964 | `	if( z[0] == '-' ){` |
|     22 | 2965 | `		for( i = 1 ; i < n ; i++ ){` |
|     14 | 2966 | `			if( z[i] < '0' \|\| z[i] > '9' ){` |
|      3 | 2967 | `				return 0;` |
|      - | 2968 | `			}` |
|     12 | 2969 | `			nDig++;` |
|      7 | 2970 | `		}` |
|     10 | 2971 | `		return nDig > 0;` |
|      - | 2972 | `	}` |
|   1460 | 2973 | `	for( ; i < n ; i++ ){` |
|   1038 | 2974 | `		if( z[i] >= '0' && z[i] <= '9' ){` |
|    836 | 2975 | `			nDig++;` |
|    622 | 2976 | `		}else if( z[i] == '.' ){` |
|     18 | 2977 | `			nDot++;` |
|     10 | 2978 | `		}else{` |
|    190 | 2979 | `			return 0;` |
|      - | 2980 | `		}` |
|    428 | 2981 | `	}` |
|    426 | 2982 | `	return nDig > 0 && nDot < 2;` |
|    313 | 2983 | `}` |
|    290 | 2984 | `static int VmIniValRunIsName(const char *z,sxu32 n)` |
|      4 | 2985 | `{` |
|      - | 2986 | `	sxu32 i;` |
|    294 | 2987 | `	if( n < 1 \|\| (unsigned char)z[0] >= 0xc0 \|\| (!SyisAlpha(z[0]) && z[0] != '_') ){` |
|     40 | 2988 | `		return 0;` |
|      - | 2989 | `	}` |
|    860 | 2990 | `	for( i = 1 ; i < n ; i++ ){` |
|    624 | 2991 | `		if( (unsigned char)z[i] >= 0xc0 \|\| (!SyisAlphaNum(z[i]) && z[i] != '_') ){` |
|     21 | 2992 | `			return 0;` |
|      - | 2993 | `		}` |
|    305 | 2994 | `	}` |
|    240 | 2995 | `	return 1;` |
|    149 | 2996 | `}` |
|      - | 2997 | `/*` |
|      - | 2998 | ` * php's INITIAL TOKENS, narrowed to the bytes that can actually be met at the` |
|      - | 2999 | `` * end of a label: the scanner hands any of its punctuation set (`:`, `.`, a`` |
|      - | 3000 | `` * quote, a paren, an arithmetic or bitwise sign, `%$!~<>?@{}`) to the parser`` |
|      - | 3001 | `` * as itself, but a `{LABEL_CHAR}+` run swallows all of them except these`` |
|      - | 3002 | `` * twelve -- plus `=` and `[`, which have statements of their own. So `a.b`,`` |
|      - | 3003 | `` * `a:b` and `a'b` are ordinary keys while `a&b` is not a key at all.`` |
|      - | 3004 | ` */` |
|      - | 3005 | `static int VmIniVarNameChar(int c);` |
|     76 | 3006 | `static int VmIniLabelStopIsToken(int c)` |
|      2 | 3007 | `{` |
|     90 | 3008 | `	return c == '&' \|\| c == '\|' \|\| c == '^' \|\| c == '$' \|\| c == '~'` |
|     44 | 3009 | `		\|\| c == '(' \|\| c == ')' \|\| c == '{' \|\| c == '}' \|\| c == '!'` |
|    105 | 3010 | `		\|\| c == '"' \|\| c == ']';` |
|      2 | 3011 | `}` |
|      - | 3012 | `/*` |
|      - | 3013 | `` * php's comment rule is `[;][^\r\n]*{NEWLINE}`: it returns END_OF_LINE and`` |
|      - | 3014 | ` * takes the line with it -- but a comment running into the end of the input` |
|      - | 3015 | ` * matches no rule at all, and the parser meets the end of file instead.` |
|      - | 3016 | ` */` |
|      4 | 3017 | `static int VmIniCommentEndsLine(const char *z,const char *zEnd)` |
|      1 | 3018 | `{` |
|     13 | 3019 | `	for( ; z < zEnd ; z++ ){` |
|     13 | 3020 | `		if( z[0] == '\n' \|\| z[0] == '\r' ){` |
|      5 | 3021 | `			return 1;` |
|      - | 3022 | `		}` |
|      5 | 3023 | `	}` |
|    ! 0 | 3024 | `	return 0;` |
|      3 | 3025 | `}` |
|      - | 3026 | `/*` |
|      - | 3027 | ` * php's INITIAL holds a rule for each of its bool words ahead of the one that` |
|      - | 3028 | ` * reads a LABEL, and no statement of its grammar starts with the token they` |
|      - | 3029 | `` * return: `on = 1` is a syntax error where `onx = 1` is the entry "onx". The`` |
|      - | 3030 | ` * word has to OPEN the run -- the rule carries no leading blanks of its own,` |
|      - | 3031 | `` * so `  on = 1` is still the entry "on" -- and its `{TABS_AND_SPACES}*` tail`` |
|      - | 3032 | ` * is what lets it outrun a LABEL, which stops dead at a TAB. flex takes the` |
|      - | 3033 | `` * longest match and, on a tie, the earliest rule, so `on\t= 1` is BOOL_TRUE,`` |
|      - | 3034 | `` * `on x = 1` is the entry "on x", and `none = 1` is BOOL_FALSE only because`` |
|      - | 3035 | ` * the four-byte word outruns the two-byte one inside it.` |
|      - | 3036 | ` *` |
|      - | 3037 | `` * nRun is the length of the `{LABEL}` run competing for the same bytes.`` |
|      - | 3038 | ` */` |
|    620 | 3039 | `static const char * VmIniStmtBoolTok(const char *z,const char *zEnd,int nRun)` |
|      4 | 3040 | `{` |
|    624 | 3041 | `	const char *zTok = 0;` |
|    624 | 3042 | `	int nBest = 0;` |
|      - | 3043 | `	sxu32 i;` |
|   5584 | 3044 | `	for( i = 0 ; i < SX_ARRAYSIZE(aIniBool) ; i++ ){` |
|      - | 3045 | `		const char *zTail;` |
|   4964 | 3046 | `		sxu32 nWord = aIniBool[i].nWord;` |
|      - | 3047 | `		int nMatch;` |
|   4964 | 3048 | `		if( (sxu32)(zEnd - z) < nWord \|\| SyStrnicmp(z,aIniBool[i].zWord,nWord) != 0 ){` |
|   4916 | 3049 | `			continue;` |
|      - | 3050 | `		}` |
|     49 | 3051 | `		zTail = &z[nWord];` |
|    115 | 3052 | `		while( zTail < zEnd && (zTail[0] == ' ' \|\| zTail[0] == '\t') ){` |
|     43 | 3053 | `			zTail++;` |
|      1 | 3054 | `		}` |
|     49 | 3055 | `		nMatch = (int)(zTail - z);` |
|     49 | 3056 | `		if( nMatch > nBest ){` |
|     49 | 3057 | `			nBest = nMatch;` |
|     49 | 3058 | `			zTok = aIniBool[i].zTok;` |
|     24 | 3059 | `		}` |
|     25 | 3060 | `	}` |
|    624 | 3061 | `	return nBest >= nRun ? zTok : 0;` |
|      4 | 3062 | `}` |
|      - | 3063 | `/*` |
|      - | 3064 | ` * Which rule eats the blank run standing at a statement position decides what` |
|      - | 3065 | ` * the statement behind it IS, because a SPACE is a LABEL_CHAR and a TAB is` |
|      - | 3066 | `` * not. `{TABS_AND_SPACES}*[=]`, the comment rule and the newline rule outrun`` |
|      - | 3067 | `` * everything; failing those, `{LABEL}` and `{LABEL}"["` swallow a SPACES-ONLY`` |
|      - | 3068 | ` * run and trim it back off the name, and only a run holding a TAB is left to` |
|      - | 3069 | `` * `{TABS_AND_SPACES}+` and thrown away whole. So `  [s]` is the OFFSET`` |
|      - | 3070 | `` * `  `[s] where `\t[s]` is the section, and `  on = 1` is the entry "on"`` |
|      - | 3071 | `` * where `\ton = 1` opens with php's BOOL_TRUE.`` |
|      - | 3072 | ` *` |
|      - | 3073 | ` * Answers the byte the next token starts at.` |
|      - | 3074 | ` */` |
|   1500 | 3075 | `static const char * VmIniStmtStart(const char *z,const char *zEnd)` |
|      4 | 3076 | `{` |
|   1504 | 3077 | `	const char *zBlank = z;` |
|   1504 | 3078 | `	int bTab = 0;` |
|   2158 | 3079 | `	while( z < zEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|     58 | 3080 | `		if( z[0] == '\t' ){` |
|     18 | 3081 | `			bTab = 1;` |
|      8 | 3082 | `		}` |
|     58 | 3083 | `		z++;` |
|      2 | 3084 | `	}` |
|   1500 | 3085 | `	if( !bTab && z > zBlank && z < zEnd` |
|     26 | 3086 | `	 && z[0] != '=' && z[0] != ';' && z[0] != '\n' && z[0] != '\r' ){` |
|     24 | 3087 | `		return zBlank;   /* the label run owns the spaces */` |
|      - | 3088 | `	}` |
|   1482 | 3089 | `	return z;` |
|    754 | 3090 | `}` |
|      - | 3091 | `/*` |
|      - | 3092 | ` * Name the token php's INITIAL reads at this position the way its parser names` |
|      - | 3093 | ` * it. Three rules overlap here and flex ranks them by length, then by the` |
|      - | 3094 | `` * order they are written in: `{LABEL}"["` is TC_OFFSET (TC_SECTION when the`` |
|      - | 3095 | ` * run in front of the bracket is empty), the bool words come next, and` |
|      - | 3096 | `` * `{LABEL}` is last -- which is why `a[x] [y]` is an unexpected TC_OFFSET and`` |
|      - | 3097 | `` * `a[x]\t[y]` an unexpected TC_SECTION. With bAfterOffset the statement is`` |
|      - | 3098 | `` * already committed -- `TC_OFFSET option_offset ']' '=' string_or_value` is`` |
|      - | 3099 | ` * the only shape an offset has -- so every name carries php's` |
|      - | 3100 | `` * `, expecting '='`, and a byte that would merely have ended a bare-label`` |
|      - | 3101 | ` * statement is refused here too.` |
|      - | 3102 | ` */` |
|     60 | 3103 | `static const char * VmIniLabelStopTok(VmIniVal *p,const char *z,const char *zEnd,int bAfterOffset)` |
|      3 | 3104 | `{` |
|      - | 3105 | `	const char *zName;` |
|     63 | 3106 | `	const char *zRun = z;` |
|     63 | 3107 | `	char *zOut = p->zOp;` |
|     83 | 3108 | `	while( zRun < zEnd && VmIniVarNameChar((unsigned char)zRun[0]) ){` |
|     22 | 3109 | `		zRun++;` |
|      2 | 3110 | `	}` |
|     63 | 3111 | `	if( zRun > z ){` |
|     12 | 3112 | `		if( zRun < zEnd && zRun[0] == '[' ){` |
|      3 | 3113 | `			zName = "TC_OFFSET";` |
|      2 | 3114 | `		}else{` |
|     10 | 3115 | `			zName = VmIniStmtBoolTok(z,zEnd,(int)(zRun - z));` |
|     10 | 3116 | `			if( zName == 0 ){` |
|      5 | 3117 | `				zName = "TC_LABEL";` |
|      2 | 3118 | `			}` |
|      2 | 3119 | `		}` |
|     58 | 3120 | `	}else if( z >= zEnd \|\| z[0] == 0 ){` |
|      3 | 3121 | `		zName = "end of file";` |
|     52 | 3122 | `	}else if( z[0] == '\n' \|\| z[0] == '\r' ){` |
|      6 | 3123 | `		zName = "END_OF_LINE";` |
|     49 | 3124 | `	}else if( z[0] == ';' ){` |
|      3 | 3125 | `		zName = VmIniCommentEndsLine(z,zEnd) ? "END_OF_LINE" : "end of file";` |
|     46 | 3126 | `	}else if( z[0] == '[' ){` |
|      9 | 3127 | `		zName = "TC_SECTION";` |
|     40 | 3128 | `	}else if( VmIniLabelStopIsToken((unsigned char)z[0]) ){` |
|     37 | 3129 | `		zName = 0;   /* the byte names itself */` |
|     19 | 3130 | `	}else{` |
|    ! 0 | 3131 | `		zName = "TC_LABEL";` |
|      - | 3132 | `	}` |
|     63 | 3133 | `	if( zName ){` |
|    261 | 3134 | `		while( zName[0] ){` |
|    237 | 3135 | `			*zOut++ = *zName++;` |
|      3 | 3136 | `		}` |
|     15 | 3137 | `	}else{` |
|     37 | 3138 | `		*zOut++ = '\'';` |
|     37 | 3139 | `		*zOut++ = z[0];` |
|     37 | 3140 | `		*zOut++ = '\'';` |
|      - | 3141 | `	}` |
|     63 | 3142 | `	if( bAfterOffset ){` |
|     31 | 3143 | `		const char *zWant = ", expecting '='";` |
|    451 | 3144 | `		while( zWant[0] ){` |
|    423 | 3145 | `			*zOut++ = *zWant++;` |
|      3 | 3146 | `		}` |
|     14 | 3147 | `	}` |
|     63 | 3148 | `	zOut[0] = 0;` |
|     63 | 3149 | `	return p->zOp;` |
|      3 | 3150 | `}` |
|    320 | 3151 | `static void VmIniValBlanks(VmIniVal *p)` |
|      4 | 3152 | `{` |
|    628 | 3153 | `	while( p->zCur < p->zEnd && (p->zCur[0] == ' ' \|\| p->zCur[0] == '\t') ){` |
|    148 | 3154 | `		p->zCur++;` |
|      4 | 3155 | `	}` |
|    324 | 3156 | `}` |
|      - | 3157 | `/* Name the token sitting at the cursor the way php's parser names it in its` |
|      - | 3158 | `` * `syntax error, unexpected ...` warning. */`` |
|     42 | 3159 | `static const char * VmIniValTokName(VmIniVal *p)` |
|      1 | 3160 | `{` |
|      - | 3161 | `	int nLen;` |
|     43 | 3162 | `	if( p->zCur >= p->zEnd ){` |
|    ! 0 | 3163 | `		return "END_OF_LINE";` |
|      - | 3164 | `	}` |
|     43 | 3165 | `	if( VmIniValIsOperator(p->zCur[0]) \|\| p->zCur[0] == '"' ){` |
|     39 | 3166 | `		p->zOp[0] = '\'';` |
|     39 | 3167 | `		p->zOp[1] = p->zCur[0];` |
|     39 | 3168 | `		p->zOp[2] = '\'';` |
|     39 | 3169 | `		p->zOp[3] = 0;` |
|     39 | 3170 | `		return p->zOp;` |
|      - | 3171 | `	}` |
|      5 | 3172 | `	if( p->zCur[0] == ' ' \|\| p->zCur[0] == '\t' ){` |
|    ! 0 | 3173 | `		return "TC_WHITESPACE";` |
|      - | 3174 | `	}` |
|      5 | 3175 | `	if( p->zCur[0] == '\'' ){` |
|    ! 0 | 3176 | `		return "TC_RAW";` |
|      - | 3177 | `	}` |
|      5 | 3178 | `	if( VmIniValRunChar(p->zCur,p->zEnd,&nLen) ){` |
|      5 | 3179 | `		const char *z = p->zCur;` |
|      - | 3180 | `		sxu32 nRun;` |
|      - | 3181 | `		int iBool;` |
|      2 | 3182 | `		do {` |
|      5 | 3183 | `			z += nLen;` |
|      5 | 3184 | `		}while( z < p->zEnd && VmIniValRunChar(z,p->zEnd,&nLen) );` |
|      5 | 3185 | `		nRun = (sxu32)(z - p->zCur);` |
|      5 | 3186 | `		iBool = VmIniValBoolWord(p->zCur,nRun);` |
|      5 | 3187 | `		if( iBool >= 0 ){` |
|    ! 0 | 3188 | `			return aIniBool[iBool].zTok;` |
|      - | 3189 | `		}` |
|      5 | 3190 | `		if( VmIniValRunIsNumber(p->zCur,nRun) ){` |
|    ! 0 | 3191 | `			return "TC_NUMBER";` |
|      - | 3192 | `		}` |
|      5 | 3193 | `		return VmIniValRunIsName(p->zCur,nRun) ? "TC_CONSTANT" : "TC_STRING";` |
|      - | 3194 | `	}` |
|    ! 0 | 3195 | `	return "END_OF_LINE";` |
|     22 | 3196 | `}` |
|    634 | 3197 | `static void VmIniResInit(VmIniRes *pRes,ph7_context *pCtx)` |
|      4 | 3198 | `{` |
|    638 | 3199 | `	SyBlobInit(&pRes->sText,&pCtx->pVm->sAllocator);` |
|    638 | 3200 | `	pRes->iNum = 0;` |
|    638 | 3201 | `	pRes->bOp = 0;` |
|    638 | 3202 | `	pRes->bNumTok = 0;` |
|    638 | 3203 | `}` |
|    634 | 3204 | `static void VmIniResRelease(VmIniRes *pRes)` |
|      4 | 3205 | `{` |
|    638 | 3206 | `	SyBlobRelease(&pRes->sText);` |
|    638 | 3207 | `}` |
|      - | 3208 | `/*` |
|      - | 3209 | ` * php's get_int_val(): atoi() over the piece's text -- 0 when the text does` |
|      - | 3210 | ` * not start with a number, and the LOW 32 BITS of what it reads when it does,` |
|      - | 3211 | `` * because atoi() reads a long and hands back an int (`4294967296\|0` is 0 and`` |
|      - | 3212 | `` * `2147483648\|0` is -2147483648). A number too wide for the long saturates`` |
|      - | 3213 | `` * first, so `99999999999999999999\|0` is -1.`` |
|      - | 3214 | ` */` |
|    254 | 3215 | `static sxi32 VmIniResInt(VmIniRes *pRes)` |
|      1 | 3216 | `{` |
|      - | 3217 | `	const char *z,*zEnd;` |
|    255 | 3218 | `	sxi64 iVal = 0;` |
|    255 | 3219 | `	int bNeg = 0;` |
|    255 | 3220 | `	if( pRes->bOp ){` |
|     39 | 3221 | `		return pRes->iNum;` |
|      - | 3222 | `	}` |
|    217 | 3223 | `	z = (const char *)SyBlobData(&pRes->sText);` |
|    217 | 3224 | `	zEnd = &z[SyBlobLength(&pRes->sText)];` |
|    325 | 3225 | `	while( z < zEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|    ! 0 | 3226 | `		z++;` |
|    ! 0 | 3227 | `	}` |
|    217 | 3228 | `	if( z < zEnd && (z[0] == '-' \|\| z[0] == '+') ){` |
|      5 | 3229 | `		bNeg = (z[0] == '-');` |
|      5 | 3230 | `		z++;` |
|      2 | 3231 | `	}` |
|    673 | 3232 | `	for( ; z < zEnd && z[0] >= '0' && z[0] <= '9' ; z++ ){` |
|    461 | 3233 | `		if( iVal > (SXI64_HIGH - (z[0] - '0')) / 10 ){` |
|      - | 3234 | `			/* strtol() stops at its own ceiling, whose low 32 bits are -1;` |
|      - | 3235 | `			 * at its floor, they are 0 */` |
|      4 | 3236 | `			return bNeg ? 0 : -1;` |
|      - | 3237 | `		}` |
|    457 | 3238 | `		iVal = iVal * 10 + (z[0] - '0');` |
|    229 | 3239 | `	}` |
|    213 | 3240 | `	return (sxi32)(bNeg ? -iVal : iVal);` |
|    128 | 3241 | `}` |
|    144 | 3242 | `static void VmIniResSetInt(VmIniRes *pRes,sxi32 iVal)` |
|      1 | 3243 | `{` |
|      - | 3244 | `	char zBuf[32];` |
|    145 | 3245 | `	int nBuf = SyBufferFormat(zBuf,sizeof(zBuf),"%d",(int)iVal);` |
|    145 | 3246 | `	SyBlobReset(&pRes->sText);` |
|    145 | 3247 | `	SyBlobAppend(&pRes->sText,zBuf,(sxu32)nBuf);` |
|    145 | 3248 | `	pRes->iNum = iVal;` |
|    145 | 3249 | `	pRes->bOp = 1;` |
|    145 | 3250 | `	pRes->bNumTok = 0;` |
|    145 | 3251 | `}` |
|      - | 3252 | `static int VmIniValQuoted(VmIniVal *p,SyBlob *pOut);` |
|      - | 3253 | `static int VmIniValRaw(VmIniVal *p,SyBlob *pOut);` |
|      - | 3254 | `static int VmIniPieces(VmIniVal *p,SyBlob *pOut,int cClose,int bConst);` |
|      - | 3255 | `/*` |
|      - | 3256 | ` * php's LABEL_CHAR, the byte set ST_VARNAME reads a name out of. Everything an` |
|      - | 3257 | `` * operator, a bracket or a line end could be is excluded, so `${a b}` is the`` |
|      - | 3258 | `` * name "a b" while `${a$b}` never finds a name at all.`` |
|      - | 3259 | ` */` |
|   2724 | 3260 | `static int VmIniVarNameChar(int c)` |
|      4 | 3261 | `{` |
|   3521 | 3262 | `	return c != '=' && c != '\n' && c != '\r' && c != '\t' && c != ';'` |
|   2144 | 3263 | `		&& c != '&' && c != '\|' && c != '^' && c != '$' && c != '~'` |
|   2108 | 3264 | `		&& c != '(' && c != ')' && c != '{' && c != '}' && c != '!'` |
|   3808 | 3265 | `		&& c != '"' && c != '[' && c != ']' && c != 0;` |
|      4 | 3266 | `}` |
|      - | 3267 | `/*` |
|      - | 3268 | `` * php's cfg_var_ref: `${NAME}` or `${NAME:-fallback}`. The name is trimmed on`` |
|      - | 3269 | ` * both sides by the scanner's own rule; the fallback is a var_string_list read` |
|      - | 3270 | `` * in ST_VAR_FALLBACK, where a `\'` and a `;` have no rule at all and end the`` |
|      - | 3271 | ` * parse. Answers 0 for the syntax errors php's parser raises.` |
|      - | 3272 | ` */` |
|     44 | 3273 | `static int VmIniValDollar(VmIniVal *p,SyBlob *pOut)` |
|      2 | 3274 | `{` |
|     46 | 3275 | `	const char *z = &p->zCur[2];` |
|     46 | 3276 | `	const char *zName = z;` |
|      - | 3277 | `	SyString sName;` |
|    282 | 3278 | `	while( z < p->zEnd && VmIniVarNameChar((unsigned char)z[0]) ){` |
|    268 | 3279 | `		if( z[0] == ':' && &z[1] < p->zEnd && z[1] == '-' ){` |
|     31 | 3280 | `			break;` |
|      - | 3281 | `		}` |
|    238 | 3282 | `		z++;` |
|      2 | 3283 | `	}` |
|     46 | 3284 | `	if( z == zName ){` |
|      - | 3285 | `		/* Nothing ST_VARNAME can read: php names whichever token it found */` |
|      7 | 3286 | `		p->zErr = p->zCur = z;` |
|      7 | 3287 | `		if( z < p->zEnd && z[0] == '}' ){` |
|      3 | 3288 | `			p->zTok = "'}', expecting TC_VARNAME";` |
|      6 | 3289 | `		}else if( z < p->zEnd && z[0] == ':' ){` |
|      3 | 3290 | `			p->zTok = "TC_FALLBACK, expecting TC_VARNAME";` |
|      2 | 3291 | `		}else{` |
|      3 | 3292 | `			p->zTok = "end of file, expecting TC_VARNAME";` |
|      - | 3293 | `		}` |
|      7 | 3294 | `		return 0;` |
|      - | 3295 | `	}` |
|     40 | 3296 | `	SyStringInitFromBuf(&sName,zName,(int)(z - zName));` |
|     40 | 3297 | `	SyStringFullTrim(&sName);` |
|     40 | 3298 | `	if( z < p->zEnd && z[0] == '}' ){` |
|     10 | 3299 | `		p->zCur = &z[1];` |
|     10 | 3300 | `		VmIniExpandDollarVar(p->pCtx,sName.zString,sName.nByte,pOut,0,0);` |
|     10 | 3301 | `		return 1;` |
|      - | 3302 | `	}` |
|     31 | 3303 | `	if( z < p->zEnd && z[0] == ':' ){` |
|      - | 3304 | `		SyBlob sFall;` |
|      - | 3305 | `		int rc;` |
|     29 | 3306 | `		p->zCur = &z[2];` |
|     29 | 3307 | `		SyBlobInit(&sFall,&p->pCtx->pVm->sAllocator);` |
|     29 | 3308 | `		rc = VmIniPieces(p,&sFall,'}',1);` |
|     29 | 3309 | `		if( rc && (p->zCur >= p->zEnd \|\| p->zCur[0] != '}') ){` |
|      3 | 3310 | `			p->zErr = p->zCur;` |
|      3 | 3311 | `			p->zTok = "end of file, expecting '}'";` |
|      3 | 3312 | `			rc = 0;` |
|      1 | 3313 | `		}` |
|     29 | 3314 | `		if( !rc ){` |
|      3 | 3315 | `			SyBlobRelease(&sFall);` |
|      3 | 3316 | `			return 0;` |
|      - | 3317 | `		}` |
|     27 | 3318 | `		p->zCur++;` |
|     40 | 3319 | `		VmIniExpandDollarVar(p->pCtx,sName.zString,sName.nByte,pOut,` |
|     26 | 3320 | `			(const char *)SyBlobData(&sFall),SyBlobLength(&sFall));` |
|     27 | 3321 | `		SyBlobRelease(&sFall);` |
|     27 | 3322 | `		return 1;` |
|      - | 3323 | `	}` |
|      3 | 3324 | `	p->zErr = p->zCur = z;` |
|      3 | 3325 | `	p->zTok = "end of file, expecting TC_FALLBACK or '}'";` |
|      3 | 3326 | `	return 0;` |
|     24 | 3327 | `}` |
|      - | 3328 | `/*` |
|      - | 3329 | `` * One double-quoted piece. php's closing-quote rule is `["]{TABS_AND_SPACES}*`,`` |
|      - | 3330 | ` * so the quote eats the blanks BEHIND it as well -- that is why a value that` |
|      - | 3331 | `` * ends `"q"  ` at end of file keeps none of them while an unquoted one keeps`` |
|      - | 3332 | ` * all of them.` |
|      - | 3333 | ` */` |
|     44 | 3334 | `static int VmIniValQuoted(VmIniVal *p,SyBlob *pOut)` |
|      4 | 3335 | `{` |
|     48 | 3336 | `	p->zCur++;` |
|     78 | 3337 | `	for(;;){` |
|    162 | 3338 | `		if( p->zCur >= p->zEnd ){` |
|    ! 0 | 3339 | `			p->zErr = p->zCur;` |
|    ! 0 | 3340 | `			p->zTok = "end of file, expecting TC_DOLLAR_CURLY or TC_QUOTED_STRING or '\"'";` |
|    ! 0 | 3341 | `			return 0;` |
|      - | 3342 | `		}` |
|    162 | 3343 | `		if( p->zCur[0] == '"' ){` |
|     48 | 3344 | `			p->zCur++;` |
|     48 | 3345 | `			break;` |
|      - | 3346 | `		}` |
|    118 | 3347 | `		if( p->zCur[0] == '$' && &p->zCur[1] < p->zEnd && p->zCur[1] == '{' ){` |
|    ! 0 | 3348 | `			if( !VmIniValDollar(p,pOut) ){` |
|    ! 0 | 3349 | `				return 0;` |
|      - | 3350 | `			}` |
|    ! 0 | 3351 | `			continue;` |
|      - | 3352 | `		}` |
|    118 | 3353 | `		if( p->zCur[0] == '\\' && &p->zCur[1] < p->zEnd ){` |
|      3 | 3354 | `			int e = (unsigned char)p->zCur[1];` |
|      - | 3355 | `			/* php collapses exactly three escapes and keeps the rest */` |
|      3 | 3356 | `			if( e == '"' \|\| e == '\\' \|\| e == '$' ){` |
|      3 | 3357 | `				SyBlobAppend(pOut,&p->zCur[1],sizeof(char));` |
|      2 | 3358 | `			}else{` |
|    ! 0 | 3359 | `				SyBlobAppend(pOut,p->zCur,2*sizeof(char));` |
|      - | 3360 | `			}` |
|      3 | 3361 | `			p->zCur += 2;` |
|      3 | 3362 | `			continue;` |
|      - | 3363 | `		}` |
|    116 | 3364 | `		SyBlobAppend(pOut,p->zCur,sizeof(char));` |
|    116 | 3365 | `		p->zCur++;` |
|      4 | 3366 | `	}` |
|     48 | 3367 | `	VmIniValBlanks(p);` |
|     48 | 3368 | `	return 1;` |
|     26 | 3369 | `}` |
|      - | 3370 | `/*` |
|      - | 3371 | `` * One single-quoted piece: SINGLE_QUOTED_CHARS is `[^']`, so nothing inside`` |
|      - | 3372 | ` * is interpreted at all.` |
|      - | 3373 | ` */` |
|      8 | 3374 | `static int VmIniValRaw(VmIniVal *p,SyBlob *pOut)` |
|      3 | 3375 | `{` |
|     11 | 3376 | `	const char *zRaw = &p->zCur[1];` |
|     11 | 3377 | `	const char *z = zRaw;` |
|     55 | 3378 | `	while( z < p->zEnd && z[0] != '\'' ){` |
|     47 | 3379 | `		z++;` |
|      3 | 3380 | `	}` |
|     11 | 3381 | `	if( z >= p->zEnd ){` |
|    ! 0 | 3382 | `		p->zErr = p->zCur;` |
|    ! 0 | 3383 | `		p->zTok = "end of file";` |
|    ! 0 | 3384 | `		return 0;` |
|      - | 3385 | `	}` |
|     11 | 3386 | `	SyBlobAppend(pOut,zRaw,(sxu32)(z - zRaw));` |
|     11 | 3387 | `	p->zCur = &z[1];` |
|     11 | 3388 | `	return 1;` |
|      7 | 3389 | `}` |
|      - | 3390 | `/*` |
|      - | 3391 | ` * One element of php's SECTION_VALUE_CHARS / FALLBACK_CHARS run. Both sets are` |
|      - | 3392 | `` * `[^$\n\r;"']` minus the byte that CLOSES the construct, plus a backslash and`` |
|      - | 3393 | `` * whatever follows it, plus a LITERAL_DOLLAR -- a `$` that does not open a`` |
|      - | 3394 | `` * `${`. All three stand for themselves, which is why `[a\]b]` is the five-byte`` |
|      - | 3395 | `` * section name `a\]b` and `[$]` swallows its own `]` and never finds another.`` |
|      - | 3396 | ` */` |
|    518 | 3397 | `static int VmIniRunElem(const char *z,const char *zEnd,int cClose)` |
|      4 | 3398 | `{` |
|    522 | 3399 | `	int c = (unsigned char)z[0];` |
|    522 | 3400 | `	if( c == '\\' ){` |
|      6 | 3401 | `		return &z[1] < zEnd ? 2 : 0;` |
|      - | 3402 | `	}` |
|    518 | 3403 | `	if( c == '$' ){` |
|      3 | 3404 | `		if( &z[1] >= zEnd \|\| z[1] == '{' ){` |
|    ! 0 | 3405 | `			return 0;` |
|      - | 3406 | `		}` |
|      3 | 3407 | `		if( z[1] == '\\' && &z[2] < zEnd ){` |
|    ! 0 | 3408 | `			return 3;` |
|      - | 3409 | `		}` |
|      3 | 3410 | `		return 2;` |
|      - | 3411 | `	}` |
|    516 | 3412 | `	if( c == '\n' \|\| c == '\r' \|\| c == ';' \|\| c == '"' \|\| c == '\'' \|\| c == cClose \|\| c == 0 ){` |
|    170 | 3413 | `		return 0;` |
|      - | 3414 | `	}` |
|    350 | 3415 | `	return 1;` |
|    263 | 3416 | `}` |
|      - | 3417 | `/*` |
|      - | 3418 | ` * The text php builds in the three scanner states that share that run shape --` |
|      - | 3419 | ` * ST_SECTION_VALUE, ST_OFFSET and ST_VAR_FALLBACK. They differ in two places` |
|      - | 3420 | `` * only: the byte that ENDS the run (`]` for a section and an offset, `}` for a`` |
|      - | 3421 | ` * fallback), and whether a run that is exactly an identifier is looked up as a` |
|      - | 3422 | `` * CONSTANT. php's grammar gives a section `constant_literal`, which keeps the`` |
|      - | 3423 | `` * name as written -- `[MYC]` is the section "MYC" even where MYC is defined --`` |
|      - | 3424 | `` * and gives the other two `constant_string`, which does not. A single-quoted`` |
|      - | 3425 | ` * raw string is a rule of the first two states and of neither the third, so a` |
|      - | 3426 | `` * `'` inside a fallback ends the parse where inside a section it opens a piece.`` |
|      - | 3427 | ` *` |
|      - | 3428 | ` * Stops at the first byte no production can take, leaving it for the caller;` |
|      - | 3429 | ` * answers 0 only for a syntax error inside a piece.` |
|      - | 3430 | ` */` |
|    186 | 3431 | `static int VmIniPieces(VmIniVal *p,SyBlob *pOut,int cClose,int bConst)` |
|      4 | 3432 | `{` |
|    366 | 3433 | `	while( p->zCur < p->zEnd ){` |
|    276 | 3434 | `		int c = (unsigned char)p->zCur[0];` |
|      - | 3435 | `		int nLen;` |
|    276 | 3436 | `		if( c == ' ' \|\| c == '\t' ){` |
|      - | 3437 | ``			/* php's opening-quote rule is `{TABS_AND_SPACES}*["]`, and re2c`` |
|      - | 3438 | `			 * takes the longest match, so blanks in FRONT of a quote belong to` |
|      - | 3439 | `			 * the quote and are gone. Behind anything else they are the head of` |
|      - | 3440 | `			 * an ordinary run -- and being inside one is what stops the name` |
|      - | 3441 | `			 * that follows them being read as a constant. */` |
|      5 | 3442 | `			const char *zSave = p->zCur;` |
|      5 | 3443 | `			VmIniValBlanks(p);` |
|      5 | 3444 | `			if( p->zCur < p->zEnd && p->zCur[0] == '"' ){` |
|    ! 0 | 3445 | `				continue;` |
|      - | 3446 | `			}` |
|      3 | 3447 | `			p->zCur = zSave;` |
|      1 | 3448 | `		}` |
|    274 | 3449 | `		if( c == '$' && &p->zCur[1] < p->zEnd && p->zCur[1] == '{' ){` |
|     13 | 3450 | `			if( !VmIniValDollar(p,pOut) ){` |
|      5 | 3451 | `				return 0;` |
|      - | 3452 | `			}` |
|      9 | 3453 | `			continue;` |
|      - | 3454 | `		}` |
|    262 | 3455 | `		if( c == '"' ){` |
|      8 | 3456 | `			if( !VmIniValQuoted(p,pOut) ){` |
|    ! 0 | 3457 | `				return 0;` |
|      - | 3458 | `			}` |
|      8 | 3459 | `			continue;` |
|      - | 3460 | `		}` |
|    256 | 3461 | `		if( c == '\'' && cClose == ']' ){` |
|      6 | 3462 | `			if( !VmIniValRaw(p,pOut) ){` |
|    ! 0 | 3463 | `				return 0;` |
|      - | 3464 | `			}` |
|      6 | 3465 | `			continue;` |
|      - | 3466 | `		}` |
|    252 | 3467 | `		nLen = VmIniRunElem(p->zCur,p->zEnd,cClose);` |
|    252 | 3468 | `		if( nLen < 1 ){` |
|     96 | 3469 | `			break;` |
|      - | 3470 | `		}` |
|      - | 3471 | `		{` |
|    160 | 3472 | `			const char *zRun = p->zCur;` |
|      - | 3473 | `			sxu32 nRun;` |
|     77 | 3474 | `			do {` |
|    356 | 3475 | `				p->zCur += nLen;` |
|    429 | 3476 | `			}while( p->zCur < p->zEnd` |
|    356 | 3477 | `			 && (nLen = VmIniRunElem(p->zCur,p->zEnd,cClose)) > 0 );` |
|    160 | 3478 | `			nRun = (sxu32)(p->zCur - zRun);` |
|    198 | 3479 | `			if( bConst && VmIniValRunIsName(zRun,nRun) ){` |
|      - | 3480 | `				ph7_value sCons;` |
|      - | 3481 | `				/* php's three case-insensitive constants are not in the` |
|      - | 3482 | `				 * engine's constant table here, and this asks the TABLE:` |
|      - | 3483 | ``				 * `true` is the string "1" and `false` and `null` are the`` |
|      - | 3484 | `				 * empty one, which as an offset takes an automatic index. The` |
|      - | 3485 | ``				 * boolean WORDS that are not constants -- `on`, `yes`, `off`,`` |
|      - | 3486 | ``				 * `no`, `none` -- stay as they are written. */`` |
|     83 | 3487 | `				if( nRun == 4 && SyStrnicmp(zRun,"true",4) == 0 ){` |
|      3 | 3488 | `					SyBlobAppend(pOut,"1",sizeof(char));` |
|      5 | 3489 | `					continue;` |
|      - | 3490 | `				}` |
|     78 | 3491 | `				if( (nRun == 5 && SyStrnicmp(zRun,"false",5) == 0)` |
|     80 | 3492 | `				 \|\| (nRun == 4 && SyStrnicmp(zRun,"null",4) == 0) ){` |
|      5 | 3493 | `					continue;` |
|      - | 3494 | `				}` |
|     79 | 3495 | `				PH7_MemObjInit(p->pCtx->pVm,&sCons);` |
|     79 | 3496 | `				if( PH7_VmQueryConstant(p->pCtx->pVm,zRun,nRun,&sCons) ){` |
|      - | 3497 | `					int nCons;` |
|      6 | 3498 | `					const char *zCons = ph7_value_to_string(&sCons,&nCons);` |
|      6 | 3499 | `					SyBlobAppend(pOut,zCons,(sxu32)nCons);` |
|      4 | 3500 | `				}else{` |
|     75 | 3501 | `					SyBlobAppend(pOut,zRun,nRun);` |
|      - | 3502 | `				}` |
|     79 | 3503 | `				PH7_MemObjRelease(&sCons);` |
|     41 | 3504 | `			}else{` |
|     80 | 3505 | `				SyBlobAppend(pOut,zRun,nRun);` |
|      - | 3506 | `			}` |
|      - | 3507 | `		}` |
|      4 | 3508 | `	}` |
|    186 | 3509 | `	return 1;` |
|     97 | 3510 | `}` |
|      - | 3511 | `/*` |
|      - | 3512 | `` * The key php's ST_OFFSET makes of the bytes between `[` and `]`. Its run set`` |
|      - | 3513 | `` * is far wider than a VALUE's -- `=`, `#`, `\|`, `(` and a blank are all`` |
|      - | 3514 | `` * ordinary bytes here, which is why `a[1\|2]` is the three-byte key "1\|2" and`` |
|      - | 3515 | ` * never the bitwise expression a value of the same text would be. The blanks in` |
|      - | 3516 | ` * FRONT of the offset are already gone -- they belong to the scanner's own` |
|      - | 3517 | `` * `{LABEL}"["{TABS_AND_SPACES}*` rule -- and the ones behind it are not, so`` |
|      - | 3518 | `` * `a[ 4 ]` is the key "4 " and never the int 4.`` |
|      - | 3519 | ` */` |
|     90 | 3520 | `static void VmIniOffsetText(ph7_context *pCtx,const char *z,const char *zEnd,SyBlob *pOut)` |
|      3 | 3521 | `{` |
|      - | 3522 | `	VmIniVal sVal;` |
|     93 | 3523 | `	SyZero(&sVal,sizeof(sVal));` |
|     93 | 3524 | `	sVal.pCtx = pCtx;` |
|     93 | 3525 | `	sVal.zCur = z;` |
|     93 | 3526 | `	sVal.zEnd = zEnd;` |
|     93 | 3527 | `	VmIniPieces(&sVal,pOut,']',1);` |
|     93 | 3528 | `}` |
|      - | 3529 | `/*` |
|      - | 3530 | ` * php's var_string_list: one or more adjacent pieces, concatenated. Returns 0` |
|      - | 3531 | ` * when there is no piece here at all, or when a piece is a boolean word (a` |
|      - | 3532 | ` * whole-value token that can never join a list).` |
|      - | 3533 | ` */` |
|    626 | 3534 | `static int VmIniValList(VmIniVal *p,VmIniRes *pRes)` |
|      4 | 3535 | `{` |
|    630 | 3536 | `	int bAny = 0;` |
|   1386 | 3537 | `	while( p->zCur < p->zEnd ){` |
|    930 | 3538 | `		int c = (unsigned char)p->zCur[0];` |
|      - | 3539 | `		int nLen;` |
|    930 | 3540 | `		if( c == ' ' \|\| c == '\t' ){` |
|    101 | 3541 | `			const char *zBlank = p->zCur;` |
|    101 | 3542 | `			VmIniValBlanks(p);` |
|      - | 3543 | `			/* php's opening-quote rule eats the blanks in front of it, so` |
|      - | 3544 | ``			 * `x "y"` is "xy" and not "x y" */`` |
|    101 | 3545 | `			if( p->zCur < p->zEnd && p->zCur[0] == '"' ){` |
|    381 | 3546 | `				continue;` |
|      - | 3547 | `			}` |
|     97 | 3548 | `			SyBlobAppend(&pRes->sText,zBlank,(sxu32)(p->zCur - zBlank));` |
|     97 | 3549 | `			bAny = 1;` |
|     97 | 3550 | `			continue;` |
|      - | 3551 | `		}` |
|    832 | 3552 | `		if( c == '"' ){` |
|     41 | 3553 | `			if( !VmIniValQuoted(p,&pRes->sText) ){` |
|    ! 0 | 3554 | `				return 0;` |
|      - | 3555 | `			}` |
|     41 | 3556 | `			bAny = 1;` |
|     41 | 3557 | `			continue;` |
|      - | 3558 | `		}` |
|    794 | 3559 | `		if( c == '\'' ){` |
|      5 | 3560 | `			if( !VmIniValRaw(p,&pRes->sText) ){` |
|    ! 0 | 3561 | `				return 0;` |
|      - | 3562 | `			}` |
|      5 | 3563 | `			bAny = 1;` |
|      5 | 3564 | `			continue;` |
|      - | 3565 | `		}` |
|    790 | 3566 | `		if( c == '$' && &p->zCur[1] < p->zEnd && p->zCur[1] == '{' ){` |
|     34 | 3567 | `			if( !VmIniValDollar(p,&pRes->sText) ){` |
|      7 | 3568 | `				return 0;` |
|      - | 3569 | `			}` |
|     28 | 3570 | `			bAny = 1;` |
|     28 | 3571 | `			continue;` |
|      - | 3572 | `		}` |
|    758 | 3573 | `		if( c == '$' && &p->zCur[1] >= p->zEnd ){` |
|      - | 3574 | ``			/* A `$` with nothing behind it is the one byte php's scanner`` |
|      - | 3575 | `			 * stops on without complaining: the value ends there, empty. */` |
|    ! 0 | 3576 | `			p->zCur++;` |
|    ! 0 | 3577 | `			bAny = 1;` |
|    ! 0 | 3578 | `			continue;` |
|      - | 3579 | `		}` |
|    758 | 3580 | `		if( VmIniValRunChar(p->zCur,p->zEnd,&nLen) ){` |
|    610 | 3581 | `			const char *zRun = p->zCur;` |
|      - | 3582 | `			sxu32 nRun;` |
|      - | 3583 | `			int iBool;` |
|    303 | 3584 | `			do {` |
|   1582 | 3585 | `				p->zCur += nLen;` |
|   1582 | 3586 | `			}while( p->zCur < p->zEnd && VmIniValRunChar(p->zCur,p->zEnd,&nLen) );` |
|    610 | 3587 | `			nRun = (sxu32)(p->zCur - zRun);` |
|    610 | 3588 | `			iBool = VmIniValBoolWord(zRun,nRun);` |
|    610 | 3589 | `			if( iBool >= 0 ){` |
|     17 | 3590 | `				p->zCur = zRun;` |
|     17 | 3591 | `				p->zErr = zRun;` |
|     17 | 3592 | `				p->zTok = aIniBool[iBool].zTok;` |
|     17 | 3593 | `				return 0;` |
|      - | 3594 | `			}` |
|    594 | 3595 | `			if( VmIniValRunIsNumber(zRun,nRun) ){` |
|    412 | 3596 | `				if( !bAny ){` |
|    404 | 3597 | `					pRes->bNumTok = 1;` |
|    200 | 3598 | `				}` |
|    412 | 3599 | `				SyBlobAppend(&pRes->sText,zRun,nRun);` |
|    390 | 3600 | `			}else if( VmIniValRunIsName(zRun,nRun) ){` |
|      - | 3601 | `				ph7_value sCons;` |
|    154 | 3602 | `				PH7_MemObjInit(p->pCtx->pVm,&sCons);` |
|    154 | 3603 | `				if( PH7_VmQueryConstant(p->pCtx->pVm,zRun,nRun,&sCons) ){` |
|      - | 3604 | `					int nCons;` |
|     30 | 3605 | `					const char *zCons = ph7_value_to_string(&sCons,&nCons);` |
|     30 | 3606 | `					SyBlobAppend(&pRes->sText,zCons,(sxu32)nCons);` |
|     16 | 3607 | `				}else{` |
|    126 | 3608 | `					SyBlobAppend(&pRes->sText,zRun,nRun);` |
|      - | 3609 | `				}` |
|    154 | 3610 | `				PH7_MemObjRelease(&sCons);` |
|     79 | 3611 | `			}else{` |
|     35 | 3612 | `				SyBlobAppend(&pRes->sText,zRun,nRun);` |
|      - | 3613 | `			}` |
|    594 | 3614 | `			bAny = 1;` |
|    594 | 3615 | `			continue;` |
|      - | 3616 | `		}` |
|    149 | 3617 | `		break;` |
|    ! 0 | 3618 | `	}` |
|    608 | 3619 | `	return bAny;` |
|    317 | 3620 | `}` |
|      - | 3621 | `static int VmIniValExpr(VmIniVal *p,VmIniRes *pRes,int nDepth);` |
|    692 | 3622 | `static int VmIniValUnary(VmIniVal *p,VmIniRes *pRes,int nDepth)` |
|      4 | 3623 | `{` |
|      - | 3624 | `	int c;` |
|    696 | 3625 | `	if( nDepth > VM_INI_VAL_MAX_DEPTH ){` |
|    ! 0 | 3626 | `		p->zErr = p->zCur;` |
|    ! 0 | 3627 | `		p->zTok = "'('";` |
|    ! 0 | 3628 | `		return 0;` |
|      - | 3629 | `	}` |
|    696 | 3630 | `	if( p->zCur >= p->zEnd ){` |
|      9 | 3631 | `		p->zErr = p->zCur;` |
|      9 | 3632 | `		p->zTok = "END_OF_LINE";` |
|      9 | 3633 | `		return 0;` |
|      - | 3634 | `	}` |
|    688 | 3635 | `	c = (unsigned char)p->zCur[0];` |
|    688 | 3636 | `	if( c == '~' \|\| c == '!' ){` |
|     39 | 3637 | `		p->zCur++;` |
|     39 | 3638 | `		VmIniValBlanks(p);   /* php's operator rule eats its own trailing blanks */` |
|     39 | 3639 | `		if( !VmIniValUnary(p,pRes,nDepth+1) ){` |
|    ! 0 | 3640 | `			return 0;` |
|      - | 3641 | `		}` |
|     39 | 3642 | `		VmIniResSetInt(pRes,c == '~' ? ~VmIniResInt(pRes) : (sxi32)(VmIniResInt(pRes) == 0));` |
|     39 | 3643 | `		return 1;` |
|      - | 3644 | `	}` |
|    650 | 3645 | `	if( c == '(' ){` |
|     21 | 3646 | `		p->zCur++;` |
|     21 | 3647 | `		VmIniValBlanks(p);` |
|     21 | 3648 | `		if( !VmIniValExpr(p,pRes,nDepth+1) ){` |
|      9 | 3649 | `			return 0;` |
|      - | 3650 | `		}` |
|     13 | 3651 | `		if( p->zCur >= p->zEnd ){` |
|      5 | 3652 | `			p->zErr = p->zCur;` |
|      5 | 3653 | `			p->zTok = "END_OF_LINE, expecting '^' or '\|' or '&' or ')'";` |
|      5 | 3654 | `			return 0;` |
|      - | 3655 | `		}` |
|      9 | 3656 | `		if( p->zCur[0] != ')' ){` |
|    ! 0 | 3657 | `			p->zErr = p->zCur;` |
|    ! 0 | 3658 | `			p->zTok = VmIniValTokName(p);` |
|    ! 0 | 3659 | `			return 0;` |
|      - | 3660 | `		}` |
|      9 | 3661 | `		p->zCur++;` |
|      9 | 3662 | `		VmIniValBlanks(p);` |
|      9 | 3663 | `		return 1;` |
|      - | 3664 | `	}` |
|    630 | 3665 | `	if( !VmIniValList(p,pRes) ){` |
|     36 | 3666 | `		if( p->zTok == 0 ){` |
|     13 | 3667 | `			p->zErr = p->zCur;` |
|     13 | 3668 | `			p->zTok = VmIniValTokName(p);` |
|      6 | 3669 | `		}` |
|     36 | 3670 | `		return 0;` |
|      - | 3671 | `	}` |
|    596 | 3672 | `	return 1;` |
|    350 | 3673 | `}` |
|      - | 3674 | ``/* php gives `\|`, `&` and `^` one precedence level and makes them left`` |
|      - | 3675 | `` * associative, so `1&2^3\|4` is `((1&2)^3)\|4`. */`` |
|    544 | 3676 | `static int VmIniValExpr(VmIniVal *p,VmIniRes *pRes,int nDepth)` |
|      4 | 3677 | `{` |
|    548 | 3678 | `	if( nDepth > VM_INI_VAL_MAX_DEPTH ){` |
|    ! 0 | 3679 | `		p->zErr = p->zCur;` |
|    ! 0 | 3680 | `		p->zTok = "'('";` |
|    ! 0 | 3681 | `		return 0;` |
|      - | 3682 | `	}` |
|    548 | 3683 | `	if( !VmIniValUnary(p,pRes,nDepth) ){` |
|     52 | 3684 | `		return 0;` |
|      - | 3685 | `	}` |
|    353 | 3686 | `	for(;;){` |
|      - | 3687 | `		VmIniRes sRhs;` |
|      - | 3688 | `		sxi32 iLhs,iRhs,iRes;` |
|      - | 3689 | `		int c,rc;` |
|    604 | 3690 | `		if( p->zCur >= p->zEnd \|\| !VmIniValIsBinOp(p->zCur[0]) ){` |
|    249 | 3691 | `			break;` |
|      - | 3692 | `		}` |
|    111 | 3693 | `		c = (unsigned char)p->zCur[0];` |
|    111 | 3694 | `		p->zCur++;` |
|    111 | 3695 | `		VmIniValBlanks(p);` |
|    111 | 3696 | `		iLhs = VmIniResInt(pRes);` |
|    111 | 3697 | `		VmIniResInit(&sRhs,p->pCtx);` |
|    111 | 3698 | `		rc = VmIniValUnary(p,&sRhs,nDepth+1);` |
|    111 | 3699 | `		iRhs = rc ? VmIniResInt(&sRhs) : 0;` |
|    111 | 3700 | `		VmIniResRelease(&sRhs);` |
|    111 | 3701 | `		if( !rc ){` |
|      5 | 3702 | `			return 0;` |
|      - | 3703 | `		}` |
|    107 | 3704 | `		iRes = c == '\|' ? (iLhs \| iRhs) : (c == '&' ? (iLhs & iRhs) : (iLhs ^ iRhs));` |
|    107 | 3705 | `		VmIniResSetInt(pRes,iRes);` |
|      1 | 3706 | `	}` |
|    494 | 3707 | `	return 1;` |
|    276 | 3708 | `}` |
|      - | 3709 | `/*` |
|      - | 3710 | ` * INI_SCANNER_TYPED's number: php re-reads the text of a NUMBER token and` |
|      - | 3711 | `` * keeps it a string when it does not fit. `-?[0-9]+` is an int, a decimal is`` |
|      - | 3712 | ` * a float, and an int too wide for one stays the source text.` |
|      - | 3713 | ` */` |
|     24 | 3714 | `static int VmIniValNormalize(SyBlob *pText,ph7_value *pValue)` |
|      2 | 3715 | `{` |
|     26 | 3716 | `	const char *z = (const char *)SyBlobData(pText);` |
|     26 | 3717 | `	sxu32 n = SyBlobLength(pText);` |
|      - | 3718 | `	sxu32 i;` |
|     26 | 3719 | `	int bDot = 0;` |
|     38 | 3720 | `	while( n > 0 && (z[n-1] == ' ' \|\| z[n-1] == '\t') ){` |
|    ! 0 | 3721 | `		n--;` |
|    ! 0 | 3722 | `	}` |
|     26 | 3723 | `	if( n < 1 \|\| !VmIniValRunIsNumber(z,n) ){` |
|      - | 3724 | ``		/* the concatenation grew past the number token: `1 2` is a string */`` |
|      3 | 3725 | `		return 0;` |
|      - | 3726 | `	}` |
|    132 | 3727 | `	for( i = (z[0] == '-') ? 1 : 0 ; i < n ; i++ ){` |
|    110 | 3728 | `		if( z[i] == '.' ){` |
|      7 | 3729 | `			bDot = 1;` |
|      3 | 3730 | `		}` |
|     56 | 3731 | `	}` |
|     24 | 3732 | `	if( bDot ){` |
|      7 | 3733 | `		double rVal = 0;` |
|      7 | 3734 | `		SyStrToReal(z,n,(void *)&rVal,0);` |
|      7 | 3735 | `		ph7_value_double(pValue,rVal);` |
|      7 | 3736 | `		return 1;` |
|    ! 0 | 3737 | `	}else{` |
|     18 | 3738 | `		sxi64 iVal = 0;` |
|     18 | 3739 | `		int iOverflow = 0;` |
|     18 | 3740 | `		SyStrToInt64Ex(z,n,(void *)&iVal,0,&iOverflow);` |
|     18 | 3741 | `		if( iOverflow ){` |
|      3 | 3742 | `			return 0;` |
|      - | 3743 | `		}` |
|     16 | 3744 | `		ph7_value_int64(pValue,iVal);` |
|     16 | 3745 | `		return 1;` |
|      - | 3746 | `	}` |
|     14 | 3747 | `}` |
|      - | 3748 | `/*` |
|      - | 3749 | ` * Interpret one ini value the way php's INI_SCANNER_NORMAL and` |
|      - | 3750 | ` * INI_SCANNER_TYPED do (RAW never reaches here). Answers 0 for the syntax` |
|      - | 3751 | ` * errors php's own parser raises, leaving the offending token in *p.` |
|      - | 3752 | ` *` |
|      - | 3753 | ` * pValue arrives as an empty string.` |
|      - | 3754 | ` */` |
|    574 | 3755 | `static int VmIniInterpretValue(ph7_context *pCtx,const SyString *pRaw,int iMode,` |
|      - | 3756 | `	ph7_value *pValue,VmIniVal *p)` |
|      4 | 3757 | `{` |
|      - | 3758 | `	VmIniRes sRes;` |
|      - | 3759 | `	int nLen;` |
|    578 | 3760 | `	p->pCtx = pCtx;` |
|    578 | 3761 | `	p->zCur = pRaw->zString;` |
|    578 | 3762 | `	p->zEnd = &pRaw->zString[pRaw->nByte];` |
|    578 | 3763 | `	p->zErr = 0;` |
|    578 | 3764 | `	p->zTok = 0;` |
|    578 | 3765 | `	if( pRaw->nByte == 0 ){` |
|    ! 0 | 3766 | `		return 1;   /* the empty string, both modes */` |
|      - | 3767 | `	}` |
|      - | 3768 | `	/* A boolean word is a token of its own: it stands as the whole value and` |
|      - | 3769 | `	 * is a syntax error anywhere else. */` |
|    578 | 3770 | `	if( VmIniValRunChar(p->zCur,p->zEnd,&nLen) ){` |
|    472 | 3771 | `		const char *z = p->zCur;` |
|      - | 3772 | `		int iBool;` |
|    234 | 3773 | `		do {` |
|   1344 | 3774 | `			z += nLen;` |
|   1344 | 3775 | `		}while( z < p->zEnd && VmIniValRunChar(z,p->zEnd,&nLen) );` |
|    472 | 3776 | `		iBool = VmIniValBoolWord(p->zCur,(sxu32)(z - p->zCur));` |
|    472 | 3777 | `		if( iBool >= 0 ){` |
|     62 | 3778 | `			while( z < p->zEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|      5 | 3779 | `				z++;` |
|      1 | 3780 | `			}` |
|     52 | 3781 | `			if( z < p->zEnd ){` |
|      9 | 3782 | `				p->zCur = z;` |
|      9 | 3783 | `				p->zErr = z;` |
|      9 | 3784 | `				p->zTok = VmIniValTokName(p);` |
|      9 | 3785 | `				return 0;` |
|      - | 3786 | `			}` |
|     44 | 3787 | `			if( iMode == PH7_INI_SCANNER_TYPED ){` |
|     28 | 3788 | `				if( aIniBool[iBool].iKind == 2 ){` |
|      6 | 3789 | `					ph7_value_null(pValue);` |
|      4 | 3790 | `				}else{` |
|     24 | 3791 | `					ph7_value_bool(pValue,aIniBool[iBool].iKind);` |
|      2 | 3792 | `				}` |
|     31 | 3793 | `			}else if( aIniBool[iBool].iKind == 1 ){` |
|      8 | 3794 | `				ph7_value_string(pValue,"1",1);` |
|      3 | 3795 | `			}` |
|      - | 3796 | `			/* NORMAL false/null: the empty string pValue already holds */` |
|     44 | 3797 | `			return 1;` |
|      - | 3798 | `		}` |
|    209 | 3799 | `	}` |
|    528 | 3800 | `	VmIniResInit(&sRes,pCtx);` |
|    528 | 3801 | `	if( !VmIniValExpr(p,&sRes,0) ){` |
|     48 | 3802 | `		VmIniResRelease(&sRes);` |
|     48 | 3803 | `		return 0;` |
|      - | 3804 | `	}` |
|    482 | 3805 | `	if( p->zCur < p->zEnd ){` |
|      - | 3806 | ``		/* A byte no production can take: `2)`, `hello!`, `something (note)` */`` |
|     23 | 3807 | `		p->zErr = p->zCur;` |
|     23 | 3808 | `		p->zTok = VmIniValTokName(p);` |
|     23 | 3809 | `		VmIniResRelease(&sRes);` |
|     23 | 3810 | `		return 0;` |
|      - | 3811 | `	}` |
|    460 | 3812 | `	if( iMode == PH7_INI_SCANNER_TYPED && sRes.bOp ){` |
|     53 | 3813 | `		ph7_value_int64(pValue,(sxi64)sRes.iNum);` |
|    431 | 3814 | `	}else if( iMode == PH7_INI_SCANNER_TYPED && sRes.bNumTok` |
|     47 | 3815 | `	 && VmIniValNormalize(&sRes.sText,pValue) ){` |
|      - | 3816 | `		/* the int or the float php's normalize_value() makes of it */` |
|    398 | 3817 | `	}else if( SyBlobLength(&sRes.sText) > 0 ){` |
|    382 | 3818 | `		ph7_value_string(pValue,(const char *)SyBlobData(&sRes.sText),(int)SyBlobLength(&sRes.sText));` |
|    189 | 3819 | `	}` |
|    460 | 3820 | `	VmIniResRelease(&sRes);` |
|    460 | 3821 | `	return 1;` |
|    291 | 3822 | `}` |
|      - | 3823 | `/*` |
|      - | 3824 | `` * php's `syntax error, unexpected <token> in <file> on line <n>` warning, and`` |
|      - | 3825 | ` * FALSE for the whole call: one bad value discards every entry parsed so far.` |
|      - | 3826 | `` * parse_ini_string() has no file to name and says `Unknown`.`` |
|      - | 3827 | ` */` |
|    194 | 3828 | `static void VmIniSyntaxError(ph7_context *pCtx,const char *zStart,const char *zFile,VmIniVal *p,` |
|      - | 3829 | `	int nLineBias)` |
|      3 | 3830 | `{` |
|      - | 3831 | `	const char *z;` |
|      - | 3832 | `	SyBlob sMsg;` |
|    197 | 3833 | `	int nLine = 1 + nLineBias;` |
|   1041 | 3834 | `	for( z = zStart ; z < p->zErr ; z++ ){` |
|    847 | 3835 | `		if( z[0] == '\n' ){` |
|     18 | 3836 | `			nLine++;` |
|      8 | 3837 | `		}` |
|    425 | 3838 | `	}` |
|    197 | 3839 | `	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|    197 | 3840 | `	SyBlobFormat(&sMsg,"syntax error, unexpected %s in %s on line %d\n",` |
|     97 | 3841 | `		p->zTok,zFile ? zFile : "Unknown",nLine);` |
|    197 | 3842 | `	if( SyBlobNullAppend(&sMsg) == SXRET_OK ){` |
|    197 | 3843 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|     97 | 3844 | `	}` |
|    197 | 3845 | `	SyBlobRelease(&sMsg);` |
|    197 | 3846 | `}` |
|    552 | 3847 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection,int iScannerMode,const char *zFile)` |
|      4 | 3848 | `{` |
|      - | 3849 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|    556 | 3850 | `	ph7_value *pEmptyArr = 0;   /* the array of the "" option name, which SyHash cannot key */` |
|    556 | 3851 | `	const char *zStart = zIn;` |
|    556 | 3852 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3853 | `	SyHashEntry *pEntry;` |
|      - | 3854 | `	SyString sEntry;` |
|      - | 3855 | `	SyHash sHash;` |
|      - | 3856 | `	VmIniVal sVal;` |
|    556 | 3857 | `	int nLineBias = 0;` |
|      - | 3858 | `	int c;` |
|      - | 3859 | `	/* Create an empty array and worker variables */` |
|    556 | 3860 | `	pArray = ph7_context_new_array(pCtx);` |
|    556 | 3861 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|    556 | 3862 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    556 | 3863 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 3864 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 3865 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3866 | `	}` |
|    556 | 3867 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|    556 | 3868 | `	SyZero(&sVal,sizeof(sVal));` |
|    556 | 3869 | `	sVal.pCtx = pCtx;` |
|    556 | 3870 | `	pCur = pArray;` |
|      - | 3871 | `	/* Start the parse process */` |
|    907 | 3872 | `	for(;;){` |
|      - | 3873 | `		/* Ignore leading white spaces -- all but the ones a LABEL claims */` |
|    597 | 3874 | `		for(;;){` |
|   1615 | 3875 | `			zIn = VmIniStmtStart(zIn,zEnd);` |
|   1476 | 3876 | `			if( zIn < zEnd && (zIn[0] == '\n' \|\| zIn[0] == '\r') ){` |
|    282 | 3877 | `				zIn++;   /* END_OF_LINE, the one rule that eats a byte by itself */` |
|    282 | 3878 | `				continue;` |
|      - | 3879 | `			}` |
|   1198 | 3880 | `			break;` |
|    ! 0 | 3881 | `		}` |
|   1198 | 3882 | `		if( zIn >= zEnd ){` |
|      - | 3883 | `			/* No more input to process */` |
|    362 | 3884 | `			break;` |
|      - | 3885 | `		}` |
|    840 | 3886 | `		if( zIn[0] == ';' ){` |
|      - | 3887 | ``			/* Comment til the end of line. `#` is NOT one: php's scanner has`` |
|      - | 3888 | `` 			 * named `;` alone since the hash form was dropped, so `# a = 1` `` |
|      - | 3889 | ``			 * is the entry "# a" and `a = 1 # tail` keeps its tail. */`` |
|     11 | 3890 | `			zIn++;` |
|     99 | 3891 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|     91 | 3892 | `				zIn++;` |
|      3 | 3893 | `			}` |
|     11 | 3894 | `			continue;` |
|      - | 3895 | `		}` |
|      - | 3896 | `		/* Reset the string cursor of the working variable */` |
|    832 | 3897 | `		ph7_value_reset_string_cursor(pWorker);` |
|    832 | 3898 | `		if( zIn[0] == '[' ){` |
|      - | 3899 | `			/* Section. php reads the name with the same grammar an option` |
|      - | 3900 | ``			 * VALUE gets -- `${...}` expansions, double-quoted pieces and`` |
|      - | 3901 | `			 * single-quoted raw ones -- and with one difference: a bare` |
|      - | 3902 | `			 * identifier is NOT looked up as a constant. Nothing is trimmed` |
|      - | 3903 | ``			 * either, so `[ a ]` is the section " a ". INI_SCANNER_RAW reads`` |
|      - | 3904 | ``			 * `[^\]\n\r]` instead and interprets none of it. */`` |
|      - | 3905 | `			SyBlob sSec;` |
|      - | 3906 | `			int rc;` |
|     82 | 3907 | `			zIn++;` |
|     82 | 3908 | `			SyBlobInit(&sSec,&pCtx->pVm->sAllocator);` |
|     82 | 3909 | `			sVal.zCur = zIn;` |
|     82 | 3910 | `			sVal.zEnd = zEnd;` |
|     82 | 3911 | `			if( iScannerMode == PH7_INI_SCANNER_RAW ){` |
|     12 | 3912 | `				zCur = zIn;` |
|     60 | 3913 | `				while( sVal.zCur < zEnd && sVal.zCur[0] != ']'` |
|     72 | 3914 | `				 && sVal.zCur[0] != '\n' && sVal.zCur[0] != '\r' ){` |
|     42 | 3915 | `					sVal.zCur++;` |
|      2 | 3916 | `				}` |
|     12 | 3917 | `				SyBlobAppend(&sSec,zCur,(sxu32)(sVal.zCur - zCur));` |
|     12 | 3918 | `				rc = 1;` |
|      7 | 3919 | `			}else{` |
|     72 | 3920 | `				rc = VmIniPieces(&sVal,&sSec,']',0);` |
|      - | 3921 | `			}` |
|     82 | 3922 | `			if( rc && (sVal.zCur >= zEnd \|\| sVal.zCur[0] != ']') ){` |
|      - | 3923 | ``				/* Nothing in either section state matches a newline, a `;` or`` |
|      - | 3924 | `				 * the end of input, so the scanner falls off the end and php` |
|      - | 3925 | `				 * names the failure the same way every time. */` |
|      7 | 3926 | `				sVal.zErr = sVal.zCur;` |
|      7 | 3927 | `				sVal.zTok = "end of file, expecting ']'";` |
|      7 | 3928 | `				rc = 0;` |
|      3 | 3929 | `			}` |
|     82 | 3930 | `			if( !rc ){` |
|     11 | 3931 | `				SyBlobRelease(&sSec);` |
|     11 | 3932 | `				goto ini_syntax_error;` |
|      - | 3933 | `			}` |
|     72 | 3934 | `			zIn = &sVal.zCur[1]; /* Trailing square bracket ']' */` |
|     72 | 3935 | `			if( bProcessSection ){` |
|      - | 3936 | `				/* An EMPTY name is a section like any other: php stores it` |
|      - | 3937 | `				 * under "" and every option behind it lands there. */` |
|     62 | 3938 | `				ph7_value_string(pWorker,(const char *)SyBlobData(&sSec),(int)SyBlobLength(&sSec));` |
|     62 | 3939 | `				pSection = ph7_context_new_array(pCtx);` |
|     62 | 3940 | `				if( pSection ){` |
|     62 | 3941 | `					ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|     62 | 3942 | `					pCur = pSection;` |
|      - | 3943 | `					/* A section is a new option namespace, so the memo of the` |
|      - | 3944 | `					 * arrays an offset has already opened cannot outlive it:` |
|      - | 3945 | ``					 * `a[x]` under [s] and `a[x]` under [t] are two entries.`` |
|      - | 3946 | `					 * Where the sections are NOT kept every option shares the` |
|      - | 3947 | ``					 * one array, and so does the memo -- `a[x]`, a section,`` |
|      - | 3948 | ``					 * then `a[y]` is one `a` holding both. */`` |
|     62 | 3949 | `					SyHashRelease(&sHash);` |
|     62 | 3950 | `					SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     62 | 3951 | `					pEmptyArr = 0;` |
|     29 | 3952 | `				}` |
|     29 | 3953 | `			}` |
|     72 | 3954 | `			SyBlobRelease(&sSec);` |
|      - | 3955 | ``			/* php's `"]"{TABS_AND_SPACES}*{NEWLINE}?` rule counts a line`` |
|      - | 3956 | `			 * whether or not it ate a newline, so a statement AFTER the` |
|      - | 3957 | `			 * bracket on the same line is reported one line further down` |
|      - | 3958 | ``			 * than it is written: `[] = 1` is an error "on line 2". */`` |
|    108 | 3959 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|      3 | 3960 | `				zIn++;` |
|      1 | 3961 | `			}` |
|     72 | 3962 | `			if( zIn < zEnd && (zIn[0] == '\n' \|\| zIn[0] == '\r') ){` |
|     66 | 3963 | `				if( zIn[0] == '\r' && &zIn[1] < zEnd && zIn[1] == '\n' ){` |
|    ! 0 | 3964 | `					zIn++;` |
|    ! 0 | 3965 | `				}` |
|     66 | 3966 | `				zIn++;` |
|     35 | 3967 | `			}else{` |
|      8 | 3968 | `				nLineBias++;` |
|      - | 3969 | `			}` |
|     38 | 3970 | `		}else{` |
|      - | 3971 | `			ph7_value *pOldCur;` |
|      - | 3972 | `			ph7_value *pOffset;` |
|      - | 3973 | `			const char *zOff,*zOffEnd,*zEq;` |
|      - | 3974 | `			int is_array;` |
|      - | 3975 | `			int iLen;` |
|      - | 3976 | `			/* Properties */` |
|    754 | 3977 | `			is_array = 0;` |
|    754 | 3978 | `			zCur = zIn;` |
|    754 | 3979 | `			iLen = 0; /* cc warning */` |
|    754 | 3980 | `			zOff = zOffEnd = 0;` |
|    754 | 3981 | `			pOffset = 0;` |
|    754 | 3982 | `			pOldCur = pCur;` |
|      - | 3983 | `			/* Scan the option name, and the offset when one is bracketed onto` |
|      - | 3984 | `			 * it. Nothing is created yet: php's grammar has no statement for a` |
|      - | 3985 | ``			 * label with no `=`, so the line may still turn out to be dropped`` |
|      - | 3986 | ``			 * whole -- and a stray `a[x]` must not leave an empty `a` behind. */`` |
|   2380 | 3987 | `			while( zIn < zEnd && VmIniVarNameChar((unsigned char)zIn[0]) ){` |
|   1630 | 3988 | `				zIn++;` |
|      4 | 3989 | `			}` |
|    754 | 3990 | `			iLen = (int)(zIn-zCur);` |
|    754 | 3991 | `			if( !(zIn < zEnd && zIn[0] == '[') ){` |
|      - | 3992 | `				/* php reads a bool word here as the VALUE token it is, and no` |
|      - | 3993 | `` 				 * statement of its grammar starts with one -- but `{LABEL}"["` `` |
|      - | 3994 | `				 * outruns the word, so an offset is still an offset. */` |
|    616 | 3995 | `				const char *zBool = VmIniStmtBoolTok(zCur,zEnd,iLen);` |
|    616 | 3996 | `				if( zBool ){` |
|     37 | 3997 | `					sVal.zErr = sVal.zCur = zCur;` |
|     37 | 3998 | `					sVal.zTok = zBool;` |
|     37 | 3999 | `					goto ini_syntax_error;` |
|      - | 4000 | `				}` |
|    288 | 4001 | `			}` |
|    718 | 4002 | `			if( zIn < zEnd && zIn[0] == '[' ){` |
|      - | 4003 | `				{` |
|      - | 4004 | `					/* Array */` |
|    141 | 4005 | `					is_array = 1;` |
|    141 | 4006 | `					zIn++;` |
|      - | 4007 | `					/* The scanner's own rule eats the blanks in front of the` |
|      - | 4008 | `					 * offset; the ones behind it are the offset's own */` |
|    214 | 4009 | `					while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|      5 | 4010 | `						zIn++;` |
|      1 | 4011 | `					}` |
|    141 | 4012 | `					zOff = zIn;` |
|      - | 4013 | ``					/* A quote inside the offset pauses the `]` that ends it,`` |
|      - | 4014 | `					 * exactly as the pushed ST_DOUBLE_QUOTES state does */` |
|    558 | 4015 | `					while( zIn < zEnd && zIn[0] != ']' && zIn[0] != '\n'` |
|    426 | 4016 | `					 && zIn[0] != '\r' && zIn[0] != ';' ){` |
|    237 | 4017 | `						if( zIn[0] == '\\' && &zIn[1] < zEnd ){` |
|      - | 4018 | `							/* SECTION_VALUE_CHARS takes a backslash and` |
|      - | 4019 | ``							 * whatever follows it as one piece, so `\]` is two`` |
|      - | 4020 | `							 * ordinary bytes and never the end of the offset */` |
|    ! 0 | 4021 | `							zIn += 2;` |
|    ! 0 | 4022 | `							continue;` |
|      - | 4023 | `						}` |
|    237 | 4024 | `						if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|      5 | 4025 | `							int q = zIn[0];` |
|      5 | 4026 | `							zIn++;` |
|     17 | 4027 | `							while( zIn < zEnd && zIn[0] != q && zIn[0] != '\n' ){` |
|     13 | 4028 | `								if( q == '"' && zIn[0] == '\\' && &zIn[1] < zEnd ){` |
|    ! 0 | 4029 | `									zIn++;` |
|    ! 0 | 4030 | `								}` |
|     13 | 4031 | `								zIn++;` |
|      1 | 4032 | `							}` |
|      5 | 4033 | `							if( zIn >= zEnd \|\| zIn[0] == '\n' ){` |
|    ! 0 | 4034 | `								break;` |
|      - | 4035 | `							}` |
|      2 | 4036 | `						}` |
|    237 | 4037 | `						zIn++;` |
|      3 | 4038 | `					}` |
|    141 | 4039 | `					zOffEnd = zIn;` |
|    141 | 4040 | `					if( zIn >= zEnd \|\| zIn[0] != ']' ){` |
|      - | 4041 | ``						/* Nothing in ST_OFFSET matches a newline, a `;` or the`` |
|      - | 4042 | `						 * end of input, so the scanner falls off the end and` |
|      - | 4043 | `						 * php names the failure the same way every time --` |
|      - | 4044 | `						 * whatever input is left behind the broken line. */` |
|      7 | 4045 | `						sVal.zErr = sVal.zCur = zOff;` |
|      7 | 4046 | `						sVal.zTok = "end of file, expecting ']'";` |
|      7 | 4047 | `						goto ini_syntax_error;` |
|      - | 4048 | `					}` |
|    135 | 4049 | `					zIn++;   /* ']' */` |
|      - | 4050 | `				}` |
|      - | 4051 | ``				/* Only `{TABS_AND_SPACES}*[=]` may stand behind the bracket,`` |
|      - | 4052 | `				 * and that rule outruns every other reading of the blanks it` |
|      - | 4053 | `				 * eats, mixed TABS and SPACES alike. */` |
|    135 | 4054 | `				zEq = zIn;` |
|    315 | 4055 | `				while( zEq < zEnd && (zEq[0] == ' ' \|\| zEq[0] == '\t') ){` |
|    117 | 4056 | `					zEq++;` |
|      3 | 4057 | `				}` |
|    135 | 4058 | `				if( zEq < zEnd && zEq[0] == '=' ){` |
|    107 | 4059 | `					zIn = zEq;` |
|     55 | 4060 | `				}else{` |
|      - | 4061 | ``					/* `TC_OFFSET option_offset ']' '='` is the only statement`` |
|      - | 4062 | ``					 * an offset has -- there is no bare `a[x]` the way there`` |
|      - | 4063 | ``					 * is a bare `a` -- so whatever stands where the `=` was`` |
|      - | 4064 | `					 * due is named and the whole parse is discarded. */` |
|     31 | 4065 | `					zIn = VmIniStmtStart(zIn,zEnd);` |
|     31 | 4066 | `					sVal.zErr = sVal.zCur = zIn;` |
|     31 | 4067 | `					sVal.zTok = VmIniLabelStopTok(&sVal,zIn,zEnd,1);` |
|     32 | 4068 | `					if( zIn < zEnd && (zIn[0] == '\n' \|\| zIn[0] == '\r'` |
|     22 | 4069 | `					 \|\| (zIn[0] == ';' && VmIniCommentEndsLine(zIn,zEnd))) ){` |
|      - | 4070 | `						/* php's END_OF_LINE rule counts the line it just ate` |
|      - | 4071 | `						 * before its parser refuses the token, so the report` |
|      - | 4072 | `						 * lands one line below the offset that caused it. */` |
|      8 | 4073 | `						nLineBias++;` |
|      3 | 4074 | `					}` |
|     31 | 4075 | `					goto ini_syntax_error;` |
|      3 | 4076 | `				}` |
|    632 | 4077 | `			}else if( zIn >= zEnd \|\| zIn[0] != '=' ){` |
|     56 | 4078 | `				zEq = zIn;` |
|     92 | 4079 | `				while( zEq < zEnd && (zEq[0] == ' ' \|\| zEq[0] == '\t') ){` |
|     12 | 4080 | `					zEq++;` |
|      2 | 4081 | `				}` |
|     56 | 4082 | `				if( zEq < zEnd && zEq[0] == '=' ){` |
|      - | 4083 | ``					/* php's `{TABS_AND_SPACES}*[=]{TABS_AND_SPACES}*` outruns`` |
|      - | 4084 | `					 * the rule that merely eats the blanks, so the TAB that` |
|      - | 4085 | `					 * ended the label does not end the statement with it:` |
|      - | 4086 | ``					 * `b<TAB>= 1` is still the entry "b". */`` |
|      8 | 4087 | `					zIn = zEq;` |
|     53 | 4088 | `				}else if( zIn < zEnd && VmIniLabelStopIsToken((unsigned char)zIn[0]) ){` |
|      - | 4089 | `					/* A byte no statement can start with. php's LABEL run` |
|      - | 4090 | `					 * stops dead at it and INITIAL hands it to the parser as` |
|      - | 4091 | ``					 * itself, so `a&b = 1` is the bare label `a` and then an`` |
|      - | 4092 | ``					 * unexpected `&` -- never the three-byte key "a&b". */`` |
|     33 | 4093 | `					sVal.zErr = sVal.zCur = zIn;` |
|     33 | 4094 | `					sVal.zTok = VmIniLabelStopTok(&sVal,zIn,zEnd,0);` |
|     33 | 4095 | `					goto ini_syntax_error;` |
|    ! 0 | 4096 | `				}else{` |
|      - | 4097 | ``					/* No `=`: php's grammar has a statement that is a bare`` |
|      - | 4098 | ``					 * TC_LABEL and does nothing, so `justaword` alone is`` |
|      - | 4099 | `					 * neither an entry nor an error, and never eats the line` |
|      - | 4100 | `					 * behind it. The scan resumes at the byte that ended the` |
|      - | 4101 | ``					 * run, which is why `a<TAB>b = 1` is the entry "b" and`` |
|      - | 4102 | `					 * not "a<TAB>b". */` |
|     18 | 4103 | `					pCur = pOldCur;` |
|     18 | 4104 | `					if( zIn < zEnd && zIn[0] == 0 ){` |
|      - | 4105 | `						/* php's scanner reads a NUL-terminated buffer and` |
|      - | 4106 | `						 * never sees the byte behind one */` |
|    ! 0 | 4107 | `						break;` |
|      - | 4108 | `					}` |
|     18 | 4109 | `					continue;` |
|      - | 4110 | `				}` |
|      3 | 4111 | `			}` |
|      - | 4112 | `			/* Trim the key. php's EAT_LEADING_WHITESPACE eats a SPACE and a` |
|      - | 4113 | `			 * TAB and nothing else, and its LABEL_CHAR set holds every other` |
|      - | 4114 | `` 			 * blank there is, so `\v` and `\f` stay part of the name: `\va = 1` `` |
|      - | 4115 | ``			 * is the entry "\va" and `\v = 1` the entry "\v". */`` |
|    638 | 4116 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    958 | 4117 | `			while( sEntry.nByte > 0` |
|    654 | 4118 | `			 && (sEntry.zString[0] == ' ' \|\| sEntry.zString[0] == '\t') ){` |
|     17 | 4119 | `				sEntry.zString++;` |
|     17 | 4120 | `				sEntry.nByte--;` |
|      1 | 4121 | `			}` |
|   1184 | 4122 | `			while( sEntry.nByte > 0` |
|   1104 | 4123 | `			 && (sEntry.zString[sEntry.nByte-1] == ' '` |
|    849 | 4124 | `			  \|\| sEntry.zString[sEntry.nByte-1] == '\t') ){` |
|    470 | 4125 | `				sEntry.nByte--;` |
|      4 | 4126 | `			}` |
|    638 | 4127 | `			if( sEntry.nByte < 1 && !is_array ){` |
|      - | 4128 | ``				/* php's grammar has no statement that starts with `=`, and`` |
|      - | 4129 | `				 * INITIAL hands the byte straight to the parser: a line whose` |
|      - | 4130 | `				 * label is empty is a syntax error and not an empty key. */` |
|      7 | 4131 | `				sVal.zErr = sVal.zCur = zIn;` |
|      7 | 4132 | `				sVal.zTok = "'='";` |
|      7 | 4133 | `				goto ini_syntax_error;` |
|      - | 4134 | `			}` |
|    632 | 4135 | `			if( sEntry.nByte > 0 \|\| is_array ){` |
|    632 | 4136 | `				if( is_array ){` |
|    107 | 4137 | `					ph7_value *pvArr = 0; /* cc warning */` |
|      - | 4138 | `					/* Query the hashtable. An option name that came out EMPTY` |
|      - | 4139 | ``					 * -- ` [x] = 1`, where the leading SPACES were the whole`` |
|      - | 4140 | `					 * label -- is an entry under "" like any other and reuses` |
|      - | 4141 | `					 * its array across lines, but SyHashGet answers no` |
|      - | 4142 | `					 * zero-length key, so that one array is memoed apart. */` |
|    107 | 4143 | `					if( sEntry.nByte < 1 ){` |
|     13 | 4144 | `						pvArr = pEmptyArr;` |
|      7 | 4145 | `					}else{` |
|     95 | 4146 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|     95 | 4147 | `						if( pEntry ){` |
|     24 | 4148 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|     11 | 4149 | `						}` |
|      - | 4150 | `					}` |
|    107 | 4151 | `					if( pvArr == 0 ){` |
|      - | 4152 | `						/* Create an empty array */` |
|     81 | 4153 | `						pvArr = ph7_context_new_array(pCtx);` |
|     81 | 4154 | `						if( pvArr ){` |
|      - | 4155 | `							/* Save the entry */` |
|     81 | 4156 | `							if( sEntry.nByte > 0 ){` |
|     73 | 4157 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|     38 | 4158 | `							}else{` |
|      9 | 4159 | `								pEmptyArr = pvArr;` |
|      - | 4160 | `							}` |
|      - | 4161 | `							/* Insert the entry */` |
|     81 | 4162 | `							ph7_value_reset_string_cursor(pWorker);` |
|     81 | 4163 | `							ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     81 | 4164 | `							ph7_array_add_elem(pCur,pWorker,pvArr);` |
|     81 | 4165 | `							ph7_value_reset_string_cursor(pWorker);` |
|     39 | 4166 | `						}` |
|     39 | 4167 | `					}` |
|    107 | 4168 | `					if( pvArr ){` |
|    107 | 4169 | `						pCur = pvArr;` |
|     52 | 4170 | `					}` |
|      - | 4171 | `					/* The offset names the entry to write. An EMPTY one -- the` |
|      - | 4172 | ``					 * `a[]` form -- is the only one that takes the next`` |
|      - | 4173 | `					 * automatic index. */` |
|    107 | 4174 | `					if( zOffEnd > zOff ){` |
|      - | 4175 | `						SyBlob sOff;` |
|     93 | 4176 | `						SyBlobInit(&sOff,&pCtx->pVm->sAllocator);` |
|     93 | 4177 | `						VmIniOffsetText(pCtx,zOff,zOffEnd,&sOff);` |
|      - | 4178 | `						/* An offset whose TEXT came out empty is the automatic` |
|      - | 4179 | ``						 * index too: `a[false]` and `a[]` are the same entry */`` |
|     93 | 4180 | `						if( SyBlobLength(&sOff) > 0 ){` |
|     89 | 4181 | `							pOffset = ph7_context_new_scalar(pCtx);` |
|     89 | 4182 | `							if( pOffset ){` |
|    132 | 4183 | `								ph7_value_string(pOffset,(const char *)SyBlobData(&sOff),` |
|     86 | 4184 | `									(int)SyBlobLength(&sOff));` |
|     43 | 4185 | `							}` |
|     43 | 4186 | `						}` |
|     93 | 4187 | `						SyBlobRelease(&sOff);` |
|     45 | 4188 | `					}` |
|     55 | 4189 | `				}else{` |
|      - | 4190 | `					/* Save the key name */` |
|    528 | 4191 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      - | 4192 | `				}` |
|      - | 4193 | `				/* extract key value. pValue must come back to an EMPTY STRING` |
|      - | 4194 | `				 * whatever the last entry typed it as (INI_SCANNER_TYPED sets` |
|      - | 4195 | `				 * bool/int/float/null): ph7_value_string() re-types it, the` |
|      - | 4196 | `				 * cursor reset then empties it. */` |
|    632 | 4197 | `				ph7_value_string(pValue,"",0);` |
|    632 | 4198 | `				ph7_value_reset_string_cursor(pValue);` |
|    632 | 4199 | `				zIn++; /* '=' */` |
|      - | 4200 | `				/* Skip the spaces BEFORE the value but never the newline that` |
|      - | 4201 | ``				 * ENDS it: `key =` at end of line is php's empty-string entry,`` |
|      - | 4202 | `				 * and the old skip ran onto the next line and swallowed it` |
|      - | 4203 | ``				 * whole — `e1 =` followed by `c1 = 10K` answered`` |
|      - | 4204 | `				 * ["e1" => "c1 = 10K"] with c1 GONE. */` |
|   1192 | 4205 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && zIn[0] != '\n' && SyisSpace(zIn[0]) ){` |
|    564 | 4206 | `					zIn++;` |
|      4 | 4207 | `				}` |
|    632 | 4208 | `				if( zIn < zEnd && zIn[0] != '\n' ){` |
|      - | 4209 | `					int bQuoted;` |
|    622 | 4210 | `					zCur = zIn;` |
|    622 | 4211 | `					c = zIn[0];` |
|    622 | 4212 | `					bQuoted = (c == '"' \|\| c == '\'');` |
|    622 | 4213 | `					if( bQuoted && iScannerMode == PH7_INI_SCANNER_RAW ){` |
|      5 | 4214 | `						zIn++;` |
|      - | 4215 | `						/* Delimit the value */` |
|     43 | 4216 | `						while( zIn < zEnd ){` |
|     43 | 4217 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|      5 | 4218 | `								break;` |
|      - | 4219 | `							}` |
|     39 | 4220 | `							zIn++;` |
|      1 | 4221 | `						}` |
|      5 | 4222 | `						if( zIn < zEnd ){` |
|      5 | 4223 | `							zIn++;` |
|      2 | 4224 | `						}` |
|      3 | 4225 | `					}else{` |
|      - | 4226 | ``						/* A quote pauses the `;` comment and the end of the`` |
|      - | 4227 | `						 * value, exactly as php's pushed ST_DOUBLE_QUOTES` |
|      - | 4228 | ``						 * state does: `a = "x" & "y"` is ONE value and not the`` |
|      - | 4229 | `						 * three bytes in front of its second quote. */` |
|    618 | 4230 | `						bQuoted = 0;` |
|   3546 | 4231 | `						while( zIn < zEnd ){` |
|   3202 | 4232 | `							if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|     50 | 4233 | `								int q = zIn[0];` |
|     50 | 4234 | `								zIn++;` |
|    182 | 4235 | `								while( zIn < zEnd && zIn[0] != q ){` |
|    136 | 4236 | `									if( q == '"' && zIn[0] == '\\' && &zIn[1] < zEnd ){` |
|    ! 0 | 4237 | `										zIn++;` |
|    ! 0 | 4238 | `									}` |
|    136 | 4239 | `									zIn++;` |
|      4 | 4240 | `								}` |
|     50 | 4241 | `								if( zIn < zEnd ){` |
|     50 | 4242 | `									zIn++;` |
|     23 | 4243 | `								}` |
|     50 | 4244 | `								continue;` |
|      - | 4245 | `							}` |
|   3156 | 4246 | `							if( zIn[0] == '\n' \|\| zIn[0] == '\r' ){` |
|      - | 4247 | `								/* php's NEWLINE is ("\r"\|"\n"\|"\r\n") */` |
|    268 | 4248 | `								if( zIn[0] == '\r' \|\| zIn[-1] != '\\' ){` |
|    136 | 4249 | `									break;` |
|    ! 0 | 4250 | `								}` |
|   2892 | 4251 | `							}else if( zIn[0] == ';' ){` |
|      - | 4252 | ``								/* Inline comments -- `;` only */`` |
|      8 | 4253 | `								break;` |
|      - | 4254 | `							}` |
|   2886 | 4255 | `							zIn++;` |
|      4 | 4256 | `						}` |
|      - | 4257 | `					}` |
|      - | 4258 | `					/* Trim the value. The blanks BEHIND it belong to the rule` |
|      - | 4259 | ``					 * that ends it (`{TABS_AND_SPACES}*{NEWLINE}`, and the`` |
|      - | 4260 | `					 * comment rule likewise), so they are the newline's and` |
|      - | 4261 | `					 * not the value's -- but a value that runs out at end of` |
|      - | 4262 | `					 * file ends on a rule with no such run, and keeps them:` |
|      - | 4263 | ``					 * `b = ends   ` unterminated is the eight-byte "ends   ". */`` |
|    622 | 4264 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|    622 | 4265 | `					SyStringLeftTrim(&sEntry);` |
|    622 | 4266 | `					if( zIn < zEnd ){` |
|    298 | 4267 | `						SyStringRightTrim(&sEntry);` |
|    137 | 4268 | `					}` |
|    622 | 4269 | `					if( bQuoted ){` |
|      9 | 4270 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|      9 | 4271 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|      2 | 4272 | `					}` |
|    622 | 4273 | `					if( iScannerMode == PH7_INI_SCANNER_RAW ){` |
|      - | 4274 | `						/* RAW keeps even a bare word uninterpreted. */` |
|     46 | 4275 | `						if( sEntry.nByte > 0 ){` |
|     46 | 4276 | `							ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     22 | 4277 | `						}` |
|     24 | 4278 | `					}else{` |
|      - | 4279 | `						VmIniVal sErr;` |
|    578 | 4280 | `						if( !VmIniInterpretValue(pCtx,&sEntry,iScannerMode,pValue,&sErr) ){` |
|      - | 4281 | `							/* php discards the whole parse over one bad value */` |
|     78 | 4282 | `							VmIniSyntaxError(pCtx,zStart,zFile,&sErr,nLineBias);` |
|     78 | 4283 | `							SyHashRelease(&sHash);` |
|     78 | 4284 | `							ph7_result_bool(pCtx,0);` |
|     78 | 4285 | `							return SXRET_OK;` |
|      - | 4286 | `						}` |
|      - | 4287 | `					}` |
|    271 | 4288 | `				}` |
|      - | 4289 | `				/* Insert the key and it's value (an empty value included) */` |
|    556 | 4290 | `				ph7_array_add_elem(pCur,is_array ? pOffset /*0: automatic index*/ : pWorker,pValue);` |
|    280 | 4291 | `			}else{` |
|    ! 0 | 4292 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 4293 | `					zIn++;` |
|    ! 0 | 4294 | `				}` |
|      - | 4295 | `			}` |
|    556 | 4296 | `			pCur = pOldCur;` |
|      - | 4297 | `		}` |
|      4 | 4298 | `	}` |
|    362 | 4299 | `	SyHashRelease(&sHash);` |
|      - | 4300 | `	/* Return the parse of the INI string */` |
|    362 | 4301 | `	ph7_result_value(pCtx,pArray);` |
|    362 | 4302 | `	return SXRET_OK;` |
|     59 | 4303 | `ini_syntax_error:` |
|      - | 4304 | `	/* php discards the whole parse over one bad line */` |
|    121 | 4305 | `	VmIniSyntaxError(pCtx,zStart,zFile,&sVal,nLineBias);` |
|    121 | 4306 | `	SyHashRelease(&sHash);` |
|    121 | 4307 | `	ph7_result_bool(pCtx,0);` |
|    121 | 4308 | `	return SXRET_OK;` |
|    280 | 4309 | `}` |
|      - | 4310 | `/*` |
|      - | 4311 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 4312 | ` *  Parse a configuration string.` |
|      - | 4313 | ` * Parameters` |
|      - | 4314 | ` *  $ini` |
|      - | 4315 | ` *   The contents of the ini file being parsed.` |
|      - | 4316 | ` *  $process_sections` |
|      - | 4317 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 4318 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 4319 | ` *  $scanner_mode` |
|      - | 4320 | ` *   INI_SCANNER_NORMAL (default: values interpreted — booleans, constants,` |
|      - | 4321 | ` *   ${var}), INI_SCANNER_RAW (values kept verbatim) or INI_SCANNER_TYPED` |
|      - | 4322 | ` *   (booleans, null and numbers come back as their own types). Any other` |
|      - | 4323 | ` *   value is php's "Invalid scanner mode" warning and FALSE.` |
|      - | 4324 | ` * Return` |
|      - | 4325 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 4326 | ` */` |
|    546 | 4327 | `PH7_PRIVATE int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 4328 | `{` |
|      - | 4329 | `	const char *zIni;` |
|      - | 4330 | `	int nByte;` |
|    550 | 4331 | `	int iMode = PH7_INI_SCANNER_NORMAL;` |
|    550 | 4332 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4333 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 4334 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4335 | `		return PH7_OK;` |
|      - | 4336 | `	}` |
|    550 | 4337 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|    463 | 4338 | `		iMode = ph7_value_to_int(apArg[2]);` |
|    460 | 4339 | `		if( iMode != PH7_INI_SCANNER_NORMAL && iMode != PH7_INI_SCANNER_RAW` |
|    132 | 4340 | `		 && iMode != PH7_INI_SCANNER_TYPED ){` |
|      - | 4341 | ``			/* php's bare message: no `func(): ` qualifier on this one */`` |
|      3 | 4342 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"Invalid scanner mode");` |
|      3 | 4343 | `			ph7_result_bool(pCtx,0);` |
|      3 | 4344 | `			return PH7_OK;` |
|      - | 4345 | `		}` |
|    229 | 4346 | `	}` |
|      - | 4347 | `	/* Extract the raw INI buffer */` |
|    548 | 4348 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 4349 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|    548 | 4350 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0,iMode,0);` |
|    277 | 4351 | `}` |
|      - | 4352 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4353 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4354 |  |
|      - | 4355 | `/*` |
|      - | 4356 | ` * Ctype Functions.` |
|      - | 4357 | ` * Status:` |
|      - | 4358 | ` *    Stable.` |
|      - | 4359 | ` */` |
|      - | 4360 | `/*` |
|      - | 4361 | ` * bool ctype_alnum(string $text)` |
|      - | 4362 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 4363 | ` * Parameters` |
|      - | 4364 | ` *  $text` |
|      - | 4365 | ` *   The tested string.` |
|      - | 4366 | ` * Return` |
|      - | 4367 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 4368 | ` */` |
|     72 | 4369 | `PH7_PRIVATE int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4370 | `{` |
|      - | 4371 | `	const unsigned char *zIn,*zEnd;` |
|      - | 4372 | `	int nLen;` |
|     73 | 4373 | `	if( nArg < 1 ){` |
|      - | 4374 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4375 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4376 | `		return PH7_OK;` |
|      - | 4377 | `	}` |
|      - | 4378 | `	/* Extract the target string */` |
|     73 | 4379 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     73 | 4380 | `	zEnd = &zIn[nLen];` |
|     73 | 4381 | `	if( nLen < 1 ){` |
|      - | 4382 | `		/* Empty string,return FALSE */` |
|      3 | 4383 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4384 | `		return PH7_OK;` |
|      - | 4385 | `	}` |
|      - | 4386 | `	/* Perform the requested operation */` |
|    110 | 4387 | `	for(;;){` |
|    221 | 4388 | `		if( zIn >= zEnd ){` |
|      - | 4389 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     65 | 4390 | `			ph7_result_bool(pCtx,1);` |
|     65 | 4391 | `			return PH7_OK;` |
|      - | 4392 | `		}` |
|    157 | 4393 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      7 | 4394 | `			break;` |
|      - | 4395 | `		}` |
|      - | 4396 | `		/* Point to the next character */` |
|    151 | 4397 | `		zIn++;` |
|      1 | 4398 | `	}` |
|      - | 4399 | `	/* The test failed,return FALSE */` |
|      7 | 4400 | `	ph7_result_bool(pCtx,0);` |
|      7 | 4401 | `	return PH7_OK;` |
|     37 | 4402 | `}` |
|      - | 4403 | `/*` |
|      - | 4404 | ` * bool ctype_alpha(string $text)` |
|      - | 4405 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 4406 | ` * Parameters` |
|      - | 4407 | ` *  $text` |
|      - | 4408 | ` *   The tested string.` |
|      - | 4409 | ` * Return` |
|      - | 4410 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 4411 | ` */` |
|     16 | 4412 | `PH7_PRIVATE int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4413 | `{` |
|      - | 4414 | `	const unsigned char *zIn,*zEnd;` |
|      - | 4415 | `	int nLen;` |
|     17 | 4416 | `	if( nArg < 1 ){` |
|      - | 4417 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4418 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4419 | `		return PH7_OK;` |
|      - | 4420 | `	}` |
|      - | 4421 | `	/* Extract the target string */` |
|     17 | 4422 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 4423 | `	zEnd = &zIn[nLen];` |
|     17 | 4424 | `	if( nLen < 1 ){` |
|      - | 4425 | `		/* Empty string,return FALSE */` |
|      3 | 4426 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4427 | `		return PH7_OK;` |
|      - | 4428 | `	}` |
|      - | 4429 | `	/* Perform the requested operation */` |
|     42 | 4430 | `	for(;;){` |
|     85 | 4431 | `		if( zIn >= zEnd ){` |
|      - | 4432 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 4433 | `			ph7_result_bool(pCtx,1);` |
|      9 | 4434 | `			return PH7_OK;` |
|      - | 4435 | `		}` |
|     77 | 4436 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 4437 | `			break;` |
|      - | 4438 | `		}` |
|      - | 4439 | `		/* Point to the next character */` |
|     71 | 4440 | `		zIn++;` |
|      1 | 4441 | `	}` |
|      - | 4442 | `	/* The test failed,return FALSE */` |
|      7 | 4443 | `	ph7_result_bool(pCtx,0);` |
|      7 | 4444 | `	return PH7_OK;` |
|      9 | 4445 | `}` |
|      - | 4446 | `/*` |
|      - | 4447 | ` * bool ctype_cntrl(string $text)` |
|      - | 4448 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 4449 | ` * Parameters` |
|      - | 4450 | ` *  $text` |
|      - | 4451 | ` *   The tested string.` |
|      - | 4452 | ` * Return` |
|      - | 4453 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 4454 | ` */` |
|     16 | 4455 | `PH7_PRIVATE int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4456 | `{` |
|      - | 4457 | `	const unsigned char *zIn,*zEnd;` |
|      - | 4458 | `	int nLen;` |
|     17 | 4459 | `	if( nArg < 1 ){` |
|      - | 4460 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4461 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4462 | `		return PH7_OK;` |
|      - | 4463 | `	}` |
|      - | 4464 | `	/* Extract the target string */` |
|     17 | 4465 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 4466 | `	zEnd = &zIn[nLen];` |
|     17 | 4467 | `	if( nLen < 1 ){` |
|      - | 4468 | `		/* Empty string,return FALSE */` |
|      3 | 4469 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4470 | `		return PH7_OK;` |
|      - | 4471 | `	}` |
|      - | 4472 | `	/* Perform the requested operation */` |
|     14 | 4473 | `	for(;;){` |
|     29 | 4474 | `		if( zIn >= zEnd ){` |
|      - | 4475 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 4476 | `			ph7_result_bool(pCtx,1);` |
|      9 | 4477 | `			return PH7_OK;` |
|      - | 4478 | `		}` |
|     21 | 4479 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 4480 | `			/* UTF-8 stream  */` |
|    ! 0 | 4481 | `			break;` |
|      - | 4482 | `		}` |
|     21 | 4483 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 4484 | `			break;` |
|      - | 4485 | `		}` |
|      - | 4486 | `		/* Point to the next character */` |
|     15 | 4487 | `		zIn++;` |
|      1 | 4488 | `	}` |
|      - | 4489 | `	/* The test failed,return FALSE */` |
|      7 | 4490 | `	ph7_result_bool(pCtx,0);` |
|      7 | 4491 | `	return PH7_OK;` |
|      9 | 4492 | `}` |
|      - | 4493 | `/*` |
|      - | 4494 | ` * bool ctype_digit(string $text)` |
|      - | 4495 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 4496 | ` * Parameters` |
|      - | 4497 | ` *  $text` |
|      - | 4498 | ` *   The tested string.` |
|      - | 4499 | ` * Return` |
|      - | 4500 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 4501 | ` */` |
|   2868 | 4502 | `PH7_PRIVATE int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 4503 | `{` |
|      - | 4504 | `	const unsigned char *zIn,*zEnd;` |
|      - | 4505 | `	int nLen;` |
|   2873 | 4506 | `	if( nArg < 1 ){` |
|      - | 4507 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4508 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4509 | `		return PH7_OK;` |
|      - | 4510 | `	}` |
|      - | 4511 | `	/* Extract the target string */` |
|   2873 | 4512 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   2873 | 4513 | `	zEnd = &zIn[nLen];` |
|   2873 | 4514 | `	if( nLen < 1 ){` |
|      - | 4515 | `		/* Empty string,return FALSE */` |
|      3 | 4516 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4517 | `		return PH7_OK;` |
|      - | 4518 | `	}` |
|      - | 4519 | `	/* Perform the requested operation */` |
|   2615 | 4520 | `	for(;;){` |
|   5235 | 4521 | `		if( zIn >= zEnd ){` |
|      - | 4522 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1703 | 4523 | `			ph7_result_bool(pCtx,1);` |
|   1703 | 4524 | `			return PH7_OK;` |
|      - | 4525 | `		}` |
|   3537 | 4526 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 4527 | `			/* UTF-8 stream  */` |
|    ! 0 | 4528 | `			break;` |
|      - | 4529 | `		}` |
|   3537 | 4530 | `		if( !SyisDigit(zIn[0]) ){` |
|   1173 | 4531 | `			break;` |
|      - | 4532 | `		}` |
|      - | 4533 | `		/* Point to the next character */` |
|   2369 | 4534 | `		zIn++;` |
|      5 | 4535 | `	}` |
|      - | 4536 | `	/* The test failed,return FALSE */` |
|   1173 | 4537 | `	ph7_result_bool(pCtx,0);` |
|   1173 | 4538 | `	return PH7_OK;` |
|   1439 | 4539 | `}` |
|      - | 4540 | `/*` |
|      - | 4541 | ` * bool ctype_xdigit(string $text)` |
|      - | 4542 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 4543 | ` * Parameters` |
|      - | 4544 | ` *  $text` |
|      - | 4545 | ` *   The tested string.` |
|      - | 4546 | ` * Return` |
|      - | 4547 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 4548 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 4549 | ` */` |
|  37234 | 4550 | `PH7_PRIVATE int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 4551 | `{` |
|      - | 4552 | `	const unsigned char *zIn,*zEnd;` |
|      - | 4553 | `	int nLen;` |
|  37238 | 4554 | `	if( nArg < 1 ){` |
|      - | 4555 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4556 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4557 | `		return PH7_OK;` |
|      - | 4558 | `	}` |
|      - | 4559 | `	/* Extract the target string */` |
|  37238 | 4560 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  37238 | 4561 | `	zEnd = &zIn[nLen];` |
|  37238 | 4562 | `	if( nLen < 1 ){` |
|      - | 4563 | `		/* Empty string,return FALSE */` |
|      3 | 4564 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4565 | `		return PH7_OK;` |
|      - | 4566 | `	}` |
|      - | 4567 | `	/* Perform the requested operation */` |
|  56456 | 4568 | `	for(;;){` |
| 111854 | 4569 | `		if( zIn >= zEnd ){` |
|      - | 4570 | `			/* If we reach the end of the string,then the test succeeded. */` |
|  37230 | 4571 | `			ph7_result_bool(pCtx,1);` |
|  37230 | 4572 | `			return PH7_OK;` |
|      - | 4573 | `		}` |
|  74628 | 4574 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 4575 | `			/* UTF-8 stream  */` |
|    ! 0 | 4576 | `			break;` |
|      - | 4577 | `		}` |
|  74628 | 4578 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 4579 | `			break;` |
|      - | 4580 | `		}` |
|      - | 4581 | `		/* Point to the next character */` |
|  74622 | 4582 | `		zIn++;` |
|      4 | 4583 | `	}` |
|      - | 4584 | `	/* The test failed,return FALSE */` |
|      7 | 4585 | `	ph7_result_bool(pCtx,0);` |
|      7 | 4586 | `	return PH7_OK;` |
|  18798 | 4587 | `}` |
|      - | 4588 | `/*` |
|      - | 4589 | ` * bool ctype_graph(string $text)` |
|      - | 4590 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 4591 | ` * Parameters` |
|      - | 4592 | ` *  $text` |
|      - | 4593 | ` *   The tested string.` |
|      - | 4594 | ` * Return` |
|      - | 4595 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 4596 | ` * (no white space), FALSE otherwise.` |
|      - | 4597 | ` */` |
|     16 | 4598 | `PH7_PRIVATE int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4599 | `{` |
|      - | 4600 | `	const unsigned char *zIn,*zEnd;` |
|      - | 4601 | `	int nLen;` |
|     17 | 4602 | `	if( nArg < 1 ){` |
|      - | 4603 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4604 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4605 | `		return PH7_OK;` |
|      - | 4606 | `	}` |
|      - | 4607 | `	/* Extract the target string */` |
|     17 | 4608 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 4609 | `	zEnd = &zIn[nLen];` |
|     17 | 4610 | `	if( nLen < 1 ){` |
|      - | 4611 | `		/* Empty string,return FALSE */` |
|      3 | 4612 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4613 | `		return PH7_OK;` |
|      - | 4614 | `	}` |
|      - | 4615 | `	/* Perform the requested operation */` |
|     57 | 4616 | `	for(;;){` |
|    115 | 4617 | `		if( zIn >= zEnd ){` |
|      - | 4618 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 4619 | `			ph7_result_bool(pCtx,1);` |
|      9 | 4620 | `			return PH7_OK;` |
|      - | 4621 | `		}` |
|    107 | 4622 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 4623 | `			/* UTF-8 stream  */` |
|    ! 0 | 4624 | `			break;` |
|      - | 4625 | `		}` |
|    107 | 4626 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 4627 | `			break;` |
|      - | 4628 | `		}` |
|      - | 4629 | `		/* Point to the next character */` |
|    101 | 4630 | `		zIn++;` |
|      1 | 4631 | `	}` |
|      - | 4632 | `	/* The test failed,return FALSE */` |
|      7 | 4633 | `	ph7_result_bool(pCtx,0);` |
|      7 | 4634 | `	return PH7_OK;` |
|      9 | 4635 | `}` |
|      - | 4636 | `/*` |
|      - | 4637 | ` * bool ctype_print(string $text)` |
|      - | 4638 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 4639 | ` * Parameters` |
|      - | 4640 | ` *  $text` |
|      - | 4641 | ` *   The tested string.` |
|      - | 4642 | ` * Return` |
|      - | 4643 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 4644 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 4645 | ` *  or control function at all.` |
|      - | 4646 | ` */` |
|    222 | 4647 | `PH7_PRIVATE int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4648 | `{` |
|      - | 4649 | `	const unsigned char *zIn,*zEnd;` |
|      - | 4650 | `	int nLen;` |
|    224 | 4651 | `	if( nArg < 1 ){` |
|      - | 4652 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4653 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4654 | `		return PH7_OK;` |
|      - | 4655 | `	}` |
|      - | 4656 | `	/* Extract the target string */` |
|    224 | 4657 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    224 | 4658 | `	zEnd = &zIn[nLen];` |
|    224 | 4659 | `	if( nLen < 1 ){` |
|      - | 4660 | `		/* Empty string,return FALSE */` |
|      6 | 4661 | `		ph7_result_bool(pCtx,0);` |
|      6 | 4662 | `		return PH7_OK;` |
|      - | 4663 | `	}` |
|      - | 4664 | `	/* Perform the requested operation */` |
|    715 | 4665 | `	for(;;){` |
|   1432 | 4666 | `		if( zIn >= zEnd ){` |
|      - | 4667 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    188 | 4668 | `			ph7_result_bool(pCtx,1);` |
|    188 | 4669 | `			return PH7_OK;` |
|      - | 4670 | `		}` |
|   1246 | 4671 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 4672 | `			/* UTF-8 stream  */` |
|    ! 0 | 4673 | `			break;` |
|      - | 4674 | `		}` |
|   1246 | 4675 | `		if( !SyisPrint(zIn[0]) ){` |
|     34 | 4676 | `			break;` |
|      - | 4677 | `		}` |
|      - | 4678 | `		/* Point to the next character */` |
|   1214 | 4679 | `		zIn++;` |
|      2 | 4680 | `	}` |
|      - | 4681 | `	/* The test failed,return FALSE */` |
|     34 | 4682 | `	ph7_result_bool(pCtx,0);` |
|     34 | 4683 | `	return PH7_OK;` |
|    113 | 4684 | `}` |
|      - | 4685 | `/*` |
|      - | 4686 | ` * bool ctype_punct(string $text)` |
|      - | 4687 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 4688 | ` * Parameters` |
|      - | 4689 | ` *  $text` |
|      - | 4690 | ` *   The tested string.` |
|      - | 4691 | ` * Return` |
|      - | 4692 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 4693 | ` *  digit or blank, FALSE otherwise.` |
|      - | 4694 | ` */` |
|     18 | 4695 | `PH7_PRIVATE int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4696 | `{` |
|      - | 4697 | `	const unsigned char *zIn,*zEnd;` |
|      - | 4698 | `	int nLen;` |
|     19 | 4699 | `	if( nArg < 1 ){` |
|      - | 4700 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4701 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4702 | `		return PH7_OK;` |
|      - | 4703 | `	}` |
|      - | 4704 | `	/* Extract the target string */` |
|     19 | 4705 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 4706 | `	zEnd = &zIn[nLen];` |
|     19 | 4707 | `	if( nLen < 1 ){` |
|      - | 4708 | `		/* Empty string,return FALSE */` |
|      3 | 4709 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4710 | `		return PH7_OK;` |
|      - | 4711 | `	}` |
|      - | 4712 | `	/* Perform the requested operation */` |
|     38 | 4713 | `	for(;;){` |
|     77 | 4714 | `		if( zIn >= zEnd ){` |
|      - | 4715 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 4716 | `			ph7_result_bool(pCtx,1);` |
|      9 | 4717 | `			return PH7_OK;` |
|      - | 4718 | `		}` |
|     69 | 4719 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 4720 | `			/* UTF-8 stream  */` |
|    ! 0 | 4721 | `			break;` |
|      - | 4722 | `		}` |
|     69 | 4723 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 4724 | `			break;` |
|      - | 4725 | `		}` |
|      - | 4726 | `		/* Point to the next character */` |
|     61 | 4727 | `		zIn++;` |
|      1 | 4728 | `	}` |
|      - | 4729 | `	/* The test failed,return FALSE */` |
|      9 | 4730 | `	ph7_result_bool(pCtx,0);` |
|      9 | 4731 | `	return PH7_OK;` |
|     10 | 4732 | `}` |
|      - | 4733 | `/*` |
|      - | 4734 | ` * bool ctype_space(string $text)` |
|      - | 4735 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 4736 | ` * Parameters` |
|      - | 4737 | ` *  $text` |
|      - | 4738 | ` *   The tested string.` |
|      - | 4739 | ` * Return` |
|      - | 4740 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 4741 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 4742 | ` *  and form feed characters.` |
|      - | 4743 | ` */` |
|  59622 | 4744 | `PH7_PRIVATE int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 4745 | `{` |
|      - | 4746 | `	const unsigned char *zIn,*zEnd;` |
|      - | 4747 | `	int nLen;` |
|  59627 | 4748 | `	if( nArg < 1 ){` |
|      - | 4749 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4750 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4751 | `		return PH7_OK;` |
|      - | 4752 | `	}` |
|      - | 4753 | `	/* Extract the target string */` |
|  59627 | 4754 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  59627 | 4755 | `	zEnd = &zIn[nLen];` |
|  59627 | 4756 | `	if( nLen < 1 ){` |
|      - | 4757 | `		/* Empty string,return FALSE */` |
|      3 | 4758 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4759 | `		return PH7_OK;` |
|      - | 4760 | `	}` |
|      - | 4761 | `	/* Perform the requested operation */` |
|  30448 | 4762 | `	for(;;){` |
|  59659 | 4763 | `		if( zIn >= zEnd ){` |
|      - | 4764 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 4765 | `			ph7_result_bool(pCtx,1);` |
|     11 | 4766 | `			return PH7_OK;` |
|      - | 4767 | `		}` |
|  59649 | 4768 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 4769 | `			/* UTF-8 stream  */` |
|    ! 0 | 4770 | `			break;` |
|      - | 4771 | `		}` |
|  59649 | 4772 | `		if( !SyisSpace(zIn[0]) ){` |
|  59615 | 4773 | `			break;` |
|      - | 4774 | `		}` |
|      - | 4775 | `		/* Point to the next character */` |
|     35 | 4776 | `		zIn++;` |
|      1 | 4777 | `	}` |
|      - | 4778 | `	/* The test failed,return FALSE */` |
|  59615 | 4779 | `	ph7_result_bool(pCtx,0);` |
|  59615 | 4780 | `	return PH7_OK;` |
|  30437 | 4781 | `}` |
|      - | 4782 | `/*` |
|      - | 4783 | ` * bool ctype_lower(string $text)` |
|      - | 4784 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 4785 | ` * Parameters` |
|      - | 4786 | ` *  $text` |
|      - | 4787 | ` *   The tested string.` |
|      - | 4788 | ` * Return` |
|      - | 4789 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 4790 | ` */` |
|     16 | 4791 | `PH7_PRIVATE int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4792 | `{` |
|      - | 4793 | `	const unsigned char *zIn,*zEnd;` |
|      - | 4794 | `	int nLen;` |
|     17 | 4795 | `	if( nArg < 1 ){` |
|      - | 4796 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4797 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4798 | `		return PH7_OK;` |
|      - | 4799 | `	}` |
|      - | 4800 | `	/* Extract the target string */` |
|     17 | 4801 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 4802 | `	zEnd = &zIn[nLen];` |
|     17 | 4803 | `	if( nLen < 1 ){` |
|      - | 4804 | `		/* Empty string,return FALSE */` |
|      3 | 4805 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4806 | `		return PH7_OK;` |
|      - | 4807 | `	}` |
|      - | 4808 | `	/* Perform the requested operation */` |
|     27 | 4809 | `	for(;;){` |
|     55 | 4810 | `		if( zIn >= zEnd ){` |
|      - | 4811 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 4812 | `			ph7_result_bool(pCtx,1);` |
|      5 | 4813 | `			return PH7_OK;` |
|      - | 4814 | `		}` |
|     51 | 4815 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 4816 | `			break;` |
|      - | 4817 | `		}` |
|      - | 4818 | `		/* Point to the next character */` |
|     41 | 4819 | `		zIn++;` |
|      1 | 4820 | `	}` |
|      - | 4821 | `	/* The test failed,return FALSE */` |
|     11 | 4822 | `	ph7_result_bool(pCtx,0);` |
|     11 | 4823 | `	return PH7_OK;` |
|      9 | 4824 | `}` |
|      - | 4825 | `/*` |
|      - | 4826 | ` * bool ctype_upper(string $text)` |
|      - | 4827 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 4828 | ` * Parameters` |
|      - | 4829 | ` *  $text` |
|      - | 4830 | ` *   The tested string.` |
|      - | 4831 | ` * Return` |
|      - | 4832 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 4833 | ` */` |
|     16 | 4834 | `PH7_PRIVATE int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4835 | `{` |
|      - | 4836 | `	const unsigned char *zIn,*zEnd;` |
|      - | 4837 | `	int nLen;` |
|     17 | 4838 | `	if( nArg < 1 ){` |
|      - | 4839 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4840 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4841 | `		return PH7_OK;` |
|      - | 4842 | `	}` |
|      - | 4843 | `	/* Extract the target string */` |
|     17 | 4844 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 4845 | `	zEnd = &zIn[nLen];` |
|     17 | 4846 | `	if( nLen < 1 ){` |
|      - | 4847 | `		/* Empty string,return FALSE */` |
|      3 | 4848 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4849 | `		return PH7_OK;` |
|      - | 4850 | `	}` |
|      - | 4851 | `	/* Perform the requested operation */` |
|     28 | 4852 | `	for(;;){` |
|     57 | 4853 | `		if( zIn >= zEnd ){` |
|      - | 4854 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 4855 | `			ph7_result_bool(pCtx,1);` |
|      5 | 4856 | `			return PH7_OK;` |
|      - | 4857 | `		}` |
|     53 | 4858 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 4859 | `			break;` |
|      - | 4860 | `		}` |
|      - | 4861 | `		/* Point to the next character */` |
|     43 | 4862 | `		zIn++;` |
|      1 | 4863 | `	}` |
|      - | 4864 | `	/* The test failed,return FALSE */` |
|     11 | 4865 | `	ph7_result_bool(pCtx,0);` |
|     11 | 4866 | `	return PH7_OK;` |
|      9 | 4867 | `}` |
|      - | 4868 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 4869 | `/*` |
|      - | 4870 | ` * Section:` |
|      - | 4871 | ` *    URL handling Functions.` |
|      - | 4872 | ` * Status:` |
|      - | 4873 | ` *    Stable.` |
|      - | 4874 | ` */` |
|      - | 4875 | `/*` |
|      - | 4876 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 4877 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 4878 | ` */` |
|   1602 | 4879 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 4880 | `{` |
|      - | 4881 | `	/* Store in the call context result buffer */` |
|   1604 | 4882 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1604 | 4883 | `	return SXRET_OK;` |
|      2 | 4884 | `}` |
|      - | 4885 | `/*` |
|      - | 4886 | ` * string base64_encode(string $data)` |
|      - | 4887 | ` *  Encodes data with MIME base64` |
|      - | 4888 | ` * Parameter` |
|      - | 4889 | ` *  $data` |
|      - | 4890 | ` *    Data to encode` |
|      - | 4891 | ` * Return` |
|      - | 4892 | ` *  Encoded data or FALSE on failure.` |
|      - | 4893 | ` */` |
|     14 | 4894 | `PH7_PRIVATE int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4895 | `{` |
|      - | 4896 | `	const char *zIn;` |
|      - | 4897 | `	int nLen;` |
|     16 | 4898 | `	if( nArg < 1 ){` |
|      - | 4899 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4900 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4901 | `		return PH7_OK;` |
|      - | 4902 | `	}` |
|      - | 4903 | `	/* Extract the input string */` |
|     16 | 4904 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     16 | 4905 | `	if( nLen < 1 ){` |
|      - | 4906 | `		/* php encodes the empty string to the EMPTY STRING; base64_encode() cannot` |
|      - | 4907 | `		 * fail at all, so FALSE was never one of its answers. */` |
|      3 | 4908 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4909 | `		return PH7_OK;` |
|      - | 4910 | `	}` |
|      - | 4911 | `	/* Perform the BASE64 encoding */` |
|     14 | 4912 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     14 | 4913 | `	return PH7_OK;` |
|      9 | 4914 | `}` |
|      - | 4915 | `/*` |
|      - | 4916 | ` * php's base64 reverse table: -1 is skippable whitespace (\t \n \r and space,` |
|      - | 4917 | ` * exactly php's set -- \v/\f are NOT skipped), -2 is an invalid byte, 0..63 the` |
|      - | 4918 | ` * decoded 6-bit value. The pad byte '=' is handled before the lookup, so its` |
|      - | 4919 | ` * table slot is never consulted.` |
|      - | 4920 | ` */` |
|      - | 4921 | `static const signed char aB64Rev[256] = {` |
|      - | 4922 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-1,-1,-2,-2,-1,-2,-2,` |
|      - | 4923 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|      - | 4924 | `	-1,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,62,-2,-2,-2,63,` |
|      - | 4925 | `	52,53,54,55,56,57,58,59,60,61,-2,-2,-2,-2,-2,-2,` |
|      - | 4926 | `	-2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,` |
|      - | 4927 | `	15,16,17,18,19,20,21,22,23,24,25,-2,-2,-2,-2,-2,` |
|      - | 4928 | `	-2,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,` |
|      - | 4929 | `	41,42,43,44,45,46,47,48,49,50,51,-2,-2,-2,-2,-2,` |
|      - | 4930 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|      - | 4931 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|      - | 4932 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|      - | 4933 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|      - | 4934 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|      - | 4935 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|      - | 4936 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|      - | 4937 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2` |
|      - | 4938 | `};` |
|      - | 4939 | `/*` |
|      - | 4940 | ` * string base64_decode(string $data, bool $strict = false)` |
|      - | 4941 | ` *  Decodes data encoded with MIME base64` |
|      - | 4942 | ` * Parameters` |
|      - | 4943 | ` *  $data` |
|      - | 4944 | ` *    Encoded data.` |
|      - | 4945 | ` *  $strict` |
|      - | 4946 | ` *    When true, return FALSE if the input contains a character outside the` |
|      - | 4947 | ` *    base64 alphabet (whitespace is still skipped) or the padding/length is` |
|      - | 4948 | ` *    malformed. When false, such bytes are silently skipped (best effort).` |
|      - | 4949 | ` * Return` |
|      - | 4950 | ` *  Returns the original data or FALSE on failure.` |
|      - | 4951 | ` * Implementation note: a faithful port of php's php_base64_decode_ex(). The old` |
|      - | 4952 | ` * code ignored $strict entirely and ran the shared SyBase64Decode(), which maps` |
|      - | 4953 | ` * every non-alphabet byte (whitespace included) to 0 rather than skipping it --` |
|      - | 4954 | ` * a silent wrong answer on padded/whitespace input in BOTH modes.` |
|      - | 4955 | ` */` |
|     46 | 4956 | `PH7_PRIVATE int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 4957 | `{` |
|      - | 4958 | `	const unsigned char *zIn;` |
|      - | 4959 | `	unsigned char *zOut;` |
|     51 | 4960 | `	int nLen,strict = 0;` |
|     51 | 4961 | `	int i = 0,j = 0,padding = 0,k;` |
|     51 | 4962 | `	if( nArg < 1 ){` |
|      - | 4963 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4964 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4965 | `		return PH7_OK;` |
|      - | 4966 | `	}` |
|      - | 4967 | `	/* Extract the input string */` |
|     51 | 4968 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     51 | 4969 | `	if( nLen < 1 ){` |
|      - | 4970 | `		/* php decodes the empty string to the EMPTY STRING, not FALSE (FALSE is reserved` |
|      - | 4971 | `		 * for input that cannot be decoded at all). */` |
|      6 | 4972 | `		ph7_result_string(pCtx,"",0);` |
|      6 | 4973 | `		return PH7_OK;` |
|      - | 4974 | `	}` |
|     46 | 4975 | `	if( nArg > 1 ){` |
|     31 | 4976 | `		strict = ph7_value_to_bool(apArg[1]);` |
|     15 | 4977 | `	}` |
|      - | 4978 | `	/* Output is at most 3/4 of the input; nLen bytes is a safe upper bound. */` |
|     46 | 4979 | `	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nLen + 1);` |
|     46 | 4980 | `	if( zOut == 0 ){` |
|    ! 0 | 4981 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4982 | `	}` |
|   3188 | 4983 | `	for( k = 0 ; k < nLen ; ++k ){` |
|   3150 | 4984 | `		int ch = zIn[k];` |
|      - | 4985 | `		int val;` |
|   3150 | 4986 | `		if( ch == '=' ){` |
|      - | 4987 | `			/* Pad byte: count it, decode nothing. */` |
|     31 | 4988 | `			padding++;` |
|     31 | 4989 | `			continue;` |
|      - | 4990 | `		}` |
|   3122 | 4991 | `		val = aB64Rev[ch];` |
|   3122 | 4992 | `		if( !strict ){` |
|      - | 4993 | `			/* Lenient: skip whitespace AND any invalid byte. */` |
|   3050 | 4994 | `			if( val < 0 ){` |
|     15 | 4995 | `				continue;` |
|      - | 4996 | `			}` |
|   1520 | 4997 | `		}else{` |
|     73 | 4998 | `			if( val == -1 ){` |
|      - | 4999 | `				/* Skippable whitespace. */` |
|      7 | 5000 | `				continue;` |
|      - | 5001 | `			}` |
|     67 | 5002 | `			if( val == -2 ){` |
|      - | 5003 | `				/* A byte outside the base64 alphabet. */` |
|      5 | 5004 | `				goto fail;` |
|      - | 5005 | `			}` |
|     63 | 5006 | `			if( padding ){` |
|      - | 5007 | `				/* Data must not follow the padding. */` |
|    ! 0 | 5008 | `				goto fail;` |
|      - | 5009 | `			}` |
|      - | 5010 | `		}` |
|   3098 | 5011 | `		switch( i & 3 ){` |
|    394 | 5012 | `			case 0:` |
|    792 | 5013 | `				zOut[j] = (unsigned char)(val << 2);` |
|    792 | 5014 | `				break;` |
|    392 | 5015 | `			case 1:` |
|    788 | 5016 | `				zOut[j++] \|= (unsigned char)(val >> 4);` |
|    788 | 5017 | `				zOut[j] = (unsigned char)((val & 0x0F) << 4);` |
|    788 | 5018 | `				break;` |
|    384 | 5019 | `			case 2:` |
|    772 | 5020 | `				zOut[j++] \|= (unsigned char)(val >> 2);` |
|    772 | 5021 | `				zOut[j] = (unsigned char)((val & 0x03) << 6);` |
|    772 | 5022 | `				break;` |
|    377 | 5023 | `			case 3:` |
|    758 | 5024 | `				zOut[j++] \|= (unsigned char)val;` |
|    754 | 5025 | `				break;` |
|      - | 5026 | `		}` |
|   3098 | 5027 | `		i++;` |
|   1551 | 5028 | `	}` |
|     42 | 5029 | `	if( strict ){` |
|      - | 5030 | `		/* A lone trailing 6-bit group (one leftover char) cannot form a byte. */` |
|     21 | 5031 | `		if( (i & 3) == 1 ){` |
|      3 | 5032 | `			goto fail;` |
|      - | 5033 | `		}` |
|      - | 5034 | `		/* Padding must be 1 or 2 bytes and complete the 4-char group. */` |
|     19 | 5035 | `		if( padding && (padding > 2 \|\| ((i + padding) & 3) != 0) ){` |
|      3 | 5036 | `			goto fail;` |
|      - | 5037 | `		}` |
|      8 | 5038 | `	}` |
|     38 | 5039 | `	ph7_result_string(pCtx,(const char *)zOut,j);` |
|     38 | 5040 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|     38 | 5041 | `	return PH7_OK;` |
|      4 | 5042 | `fail:` |
|      9 | 5043 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|      9 | 5044 | `	ph7_result_bool(pCtx,0);` |
|      9 | 5045 | `	return PH7_OK;` |
|     28 | 5046 | `}` |
|      - | 5047 | `/*` |
|      - | 5048 | ` * uuencode's six-bit alphabet: a value of 0 is written as the backtick php uses` |
|      - | 5049 | ` * instead of the historical space, every other value as ' ' + value. The three` |
|      - | 5050 | ` * PH7_UU_ENC_C* helpers pack the 6-bit groups exactly like php's macros: each` |
|      - | 5051 | ` * contribution is masked to its own bit window, so the result never depends on` |
|      - | 5052 | ` * whether the platform's char is signed.` |
|      - | 5053 | ` */` |
|      - | 5054 | ``#define PH7_UU_ENC(c)      ((char)((c) ? (((c) & 077) + ' ') : '`'))`` |
|      - | 5055 | `#define PH7_UU_ENC_C1(a)   PH7_UU_ENC((a) >> 2)` |
|      - | 5056 | `#define PH7_UU_ENC_C2(a,b) PH7_UU_ENC((((a) << 4) & 060) \| (((b) >> 4) & 017))` |
|      - | 5057 | `#define PH7_UU_ENC_C3(b,c) PH7_UU_ENC((((b) << 2) & 074) \| (((c) >> 6) & 003))` |
|      - | 5058 | `#define PH7_UU_ENC_C4(c)   PH7_UU_ENC((c) & 077)` |
|      - | 5059 | `#define PH7_UU_DEC(c)      ((((int)(c)) - ' ') & 077)` |
|      - | 5060 | `/*` |
|      - | 5061 | ` * string convert_uuencode(string $data)` |
|      - | 5062 | ` *  Uuencode a string.` |
|      - | 5063 | ` * Parameter` |
|      - | 5064 | ` *  $data` |
|      - | 5065 | ` *   Data to encode.` |
|      - | 5066 | ` * Return` |
|      - | 5067 | ` *  The uuencoded data: 45-byte lines, each prefixed with its encoded length and` |
|      - | 5068 | `` *  terminated by a newline, followed by php's "`\n" end marker. An empty input`` |
|      - | 5069 | ` *  answers just that marker.` |
|      - | 5070 | ` * Implementation note: a faithful port of php's php_uuencode(). This used to be` |
|      - | 5071 | ` * registered as an ALIAS of base64_encode() -- a wrong ALGORITHM, so every answer` |
|      - | 5072 | ` * was silently a base64 string (convert_uuencode("abc") gave "YWJj" where php` |
|      - | 5073 | `` * gives "#86)C\n`\n").`` |
|      - | 5074 | ` */` |
|     36 | 5075 | `PH7_PRIVATE int PH7_builtin_convert_uuencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5076 | `{` |
|      - | 5077 | `	const unsigned char *zIn,*zEnd,*zStop;` |
|      - | 5078 | `	char zLine[64]; /* one full line is 1 length byte + 60 data bytes + '\n' */` |
|     38 | 5079 | `	int nLen,iLen = 45,n;` |
|     38 | 5080 | `	if( nArg < 1 ){` |
|      - | 5081 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 5082 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5083 | `		return PH7_OK;` |
|      - | 5084 | `	}` |
|      - | 5085 | `	/* Extract the input string */` |
|     38 | 5086 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     38 | 5087 | `	if( nLen < 0 ){` |
|    ! 0 | 5088 | `		nLen = 0;` |
|    ! 0 | 5089 | `	}` |
|     38 | 5090 | `	zEnd = &zIn[nLen];` |
|      - | 5091 | `	/* Emit whole groups while at least four bytes remain: the last line is closed by` |
|      - | 5092 | ``	 * the tail block below so a group of one or two bytes gets php's '`' filler. */`` |
|     70 | 5093 | `	while( &zIn[3] < zEnd ){` |
|     34 | 5094 | `		zStop = &zIn[iLen];` |
|     34 | 5095 | `		if( zStop > zEnd ){` |
|      - | 5096 | `			/* A short final line: its length byte counts every remaining byte, but only` |
|      - | 5097 | `			 * whole three-byte groups are encoded here -- the leftovers ride the tail` |
|      - | 5098 | `			 * block, which then adds no length byte of its own. */` |
|     10 | 5099 | `			iLen = (int)(zEnd - zIn);` |
|     10 | 5100 | `			zStop = &zIn[(iLen/3)*3];` |
|      4 | 5101 | `		}` |
|     34 | 5102 | `		n = 0;` |
|     34 | 5103 | `		zLine[n++] = PH7_UU_ENC(iLen);` |
|    476 | 5104 | `		while( zIn < zStop ){` |
|    444 | 5105 | `			zLine[n++] = PH7_UU_ENC_C1(zIn[0]);` |
|    444 | 5106 | `			zLine[n++] = PH7_UU_ENC_C2(zIn[0],zIn[1]);` |
|    444 | 5107 | `			zLine[n++] = PH7_UU_ENC_C3(zIn[1],zIn[2]);` |
|    444 | 5108 | `			zLine[n++] = PH7_UU_ENC_C4(zIn[2]);` |
|    444 | 5109 | `			zIn += 3;` |
|      2 | 5110 | `		}` |
|     34 | 5111 | `		if( iLen == 45 ){` |
|     25 | 5112 | `			zLine[n++] = '\n';` |
|     12 | 5113 | `		}` |
|     34 | 5114 | `		ph7_result_string(pCtx,zLine,n);` |
|      2 | 5115 | `	}` |
|     38 | 5116 | `	if( zIn < zEnd ){` |
|      - | 5117 | `		/* One to three trailing bytes. php reads the bytes past the end of the string` |
|      - | 5118 | `		 * (its buffers are NUL terminated); the missing ones are zero here. */` |
|     30 | 5119 | `		unsigned char c0 = zIn[0];` |
|     30 | 5120 | `		unsigned char c1 = (&zIn[1] < zEnd) ? zIn[1] : 0;` |
|     30 | 5121 | `		unsigned char c2 = (&zIn[2] < zEnd) ? zIn[2] : 0;` |
|     30 | 5122 | `		n = 0;` |
|     30 | 5123 | `		if( iLen == 45 ){` |
|      - | 5124 | `			/* No short line was opened above: this group is a line of its own. */` |
|     22 | 5125 | `			zLine[n++] = PH7_UU_ENC((int)(zEnd - zIn));` |
|     22 | 5126 | `			iLen = 0;` |
|     10 | 5127 | `		}` |
|     30 | 5128 | `		zLine[n++] = PH7_UU_ENC_C1(c0);` |
|     30 | 5129 | `		zLine[n++] = PH7_UU_ENC_C2(c0,c1);` |
|     30 | 5130 | ``		zLine[n++] = ((zEnd - zIn) > 1) ? PH7_UU_ENC_C3(c1,c2) : '`';`` |
|     30 | 5131 | ``		zLine[n++] = ((zEnd - zIn) > 2) ? PH7_UU_ENC_C4(c2)     : '`';`` |
|     30 | 5132 | `		ph7_result_string(pCtx,zLine,n);` |
|     14 | 5133 | `	}` |
|     38 | 5134 | `	if( iLen != 45 ){` |
|      - | 5135 | `		/* A short (or tail) line is still open; a run of whole 45-byte lines -- and the` |
|      - | 5136 | `		 * empty input, which opens no line at all -- is already newline-terminated. */` |
|     30 | 5137 | `		ph7_result_string(pCtx,"\n",1);` |
|     14 | 5138 | `	}` |
|      - | 5139 | `	/* php's end marker: a zero-length line. */` |
|     38 | 5140 | ``	ph7_result_string(pCtx,"`\n",2);`` |
|     38 | 5141 | `	return PH7_OK;` |
|     20 | 5142 | `}` |
|      - | 5143 | `/*` |
|      - | 5144 | ` * string\|false convert_uudecode(string $data)` |
|      - | 5145 | ` *  Decode a uuencoded string.` |
|      - | 5146 | ` * Parameter` |
|      - | 5147 | ` *  $data` |
|      - | 5148 | ` *   Uuencoded data.` |
|      - | 5149 | ` * Return` |
|      - | 5150 | ` *  The decoded data, or FALSE (with a warning) when $data is not a valid uuencoded` |
|      - | 5151 | ` *  string: an empty input, a line claiming more bytes than the whole input holds, or` |
|      - | 5152 | ` *  a line whose data is truncated. Trailing garbage after the first short line is` |
|      - | 5153 | ` *  ignored, exactly like php.` |
|      - | 5154 | ` * Implementation note: a faithful port of php's php_uudecode(); see the encoder above` |
|      - | 5155 | ` * for why this was not a decoder at all before.` |
|      - | 5156 | ` */` |
|     48 | 5157 | `PH7_PRIVATE int PH7_builtin_convert_uudecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5158 | `{` |
|      - | 5159 | `	const unsigned char *zIn,*zEnd,*zStop;` |
|      - | 5160 | `	unsigned char *zOut;` |
|      - | 5161 | `	int nLen,iLen;` |
|     50 | 5162 | `	sxu32 nOut = 0,nTotal = 0;` |
|     50 | 5163 | `	if( nArg < 1 ){` |
|      - | 5164 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 5165 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5166 | `		return PH7_OK;` |
|      - | 5167 | `	}` |
|      - | 5168 | `	/* Extract the input string */` |
|     50 | 5169 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     50 | 5170 | `	if( nLen < 1 ){` |
|      - | 5171 | `		/* php refuses the empty string rather than decoding it to "". */` |
|      3 | 5172 | `		goto fail;` |
|      - | 5173 | `	}` |
|     48 | 5174 | `	zEnd = &zIn[nLen];` |
|      - | 5175 | `	/* Every four input characters yield three bytes and each line spends one more` |
|      - | 5176 | `	 * character on its length, so the input length is a safe upper bound. */` |
|     48 | 5177 | `	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nLen + 1);` |
|     48 | 5178 | `	if( zOut == 0 ){` |
|    ! 0 | 5179 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5180 | `	}` |
|     72 | 5181 | `	while( zIn < zEnd ){` |
|     72 | 5182 | `		iLen = PH7_UU_DEC(*zIn++);` |
|     72 | 5183 | `		if( iLen == 0 ){` |
|      - | 5184 | `			/* The end marker (or any line claiming zero bytes) stops the decoding. */` |
|     12 | 5185 | `			break;` |
|      - | 5186 | `		}` |
|     62 | 5187 | `		if( iLen > nLen ){` |
|      5 | 5188 | `			goto err;` |
|      - | 5189 | `		}` |
|     58 | 5190 | `		nTotal += (sxu32)iLen;` |
|      - | 5191 | `		/* A line carries four characters per three-byte group, whole groups only. */` |
|     58 | 5192 | `		zStop = zIn + ((iLen + 2)/3)*4;` |
|     58 | 5193 | `		if( zStop > zEnd ){` |
|      5 | 5194 | `			goto err;` |
|      - | 5195 | `		}` |
|    544 | 5196 | `		while( zIn < zStop ){` |
|    492 | 5197 | `			zOut[nOut++] = (unsigned char)((PH7_UU_DEC(zIn[0]) << 2) \| (PH7_UU_DEC(zIn[1]) >> 4));` |
|    492 | 5198 | `			zOut[nOut++] = (unsigned char)((PH7_UU_DEC(zIn[1]) << 4) \| (PH7_UU_DEC(zIn[2]) >> 2));` |
|    492 | 5199 | `			zOut[nOut++] = (unsigned char)((PH7_UU_DEC(zIn[2]) << 6) \|  PH7_UU_DEC(zIn[3]));` |
|    492 | 5200 | `			zIn += 4;` |
|      2 | 5201 | `		}` |
|     54 | 5202 | `		if( iLen < 45 ){` |
|      - | 5203 | `			/* A short line ends the payload; whatever follows is ignored. */` |
|     30 | 5204 | `			break;` |
|      - | 5205 | `		}` |
|     25 | 5206 | `		zIn++; /* Skip the line separator */` |
|      1 | 5207 | `	}` |
|      - | 5208 | `	/* Drop the padding the last group carried: php keeps only as many bytes as the` |
|      - | 5209 | `	 * length bytes declared, counted over the WHOLE input rather than per line. */` |
|     40 | 5210 | `	if( nOut > nTotal ){` |
|     20 | 5211 | `		nOut = nTotal;` |
|      9 | 5212 | `	}` |
|     40 | 5213 | `	ph7_result_string(pCtx,(const char *)zOut,(int)nOut);` |
|     40 | 5214 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|     40 | 5215 | `	return PH7_OK;` |
|      4 | 5216 | `err:` |
|      9 | 5217 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|      5 | 5218 | `fail:` |
|     11 | 5219 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5220 | `		"Argument #1 ($data) is not a valid uuencoded string"); /* the "convert_uudecode(): " prefix is added by the handler */` |
|     11 | 5221 | `	ph7_result_bool(pCtx,0);` |
|     11 | 5222 | `	return PH7_OK;` |
|     26 | 5223 | `}` |
|      - | 5224 | `/*` |
|      - | 5225 | ` * string urlencode(string $str)` |
|      - | 5226 | ` *  URL encoding` |
|      - | 5227 | ` * Parameter` |
|      - | 5228 | ` *  $data` |
|      - | 5229 | ` *   Input string.` |
|      - | 5230 | ` * Return` |
|      - | 5231 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 5232 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 5233 | ` *  encoded as plus (+) signs.` |
|      - | 5234 | ` */` |
|     16 | 5235 | `PH7_PRIVATE int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5236 | `{` |
|      - | 5237 | `	const char *zIn;` |
|      - | 5238 | `	int nLen;` |
|     18 | 5239 | `	if( nArg < 1 ){` |
|      - | 5240 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 5241 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5242 | `		return PH7_OK;` |
|      - | 5243 | `	}` |
|      - | 5244 | `	/* Extract the input string */` |
|     18 | 5245 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     18 | 5246 | `	if( nLen < 1 ){` |
|      - | 5247 | `		/* php returns an empty string for empty input, not FALSE */` |
|      6 | 5248 | `		ph7_result_string(pCtx,"",0);` |
|      6 | 5249 | `		return PH7_OK;` |
|      - | 5250 | `	}` |
|      - | 5251 | `	/* Perform the URL encoding */` |
|     14 | 5252 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     14 | 5253 | `	return PH7_OK;` |
|     10 | 5254 | `}` |
|      - | 5255 | `/*` |
|      - | 5256 | ` * string rawurlencode(string $str)` |
|      - | 5257 | ` *  RFC 3986 URL encoding: spaces become %20 (not '+') and '~' is left intact.` |
|      - | 5258 | ` */` |
|     14 | 5259 | `PH7_PRIVATE int PH7_builtin_rawurlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5260 | `{` |
|      - | 5261 | `	const char *zIn;` |
|      - | 5262 | `	int nLen;` |
|     16 | 5263 | `	if( nArg < 1 ){` |
|      - | 5264 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 5265 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5266 | `		return PH7_OK;` |
|      - | 5267 | `	}` |
|      - | 5268 | `	/* Extract the input string */` |
|     16 | 5269 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     16 | 5270 | `	if( nLen < 1 ){` |
|      - | 5271 | `		/* php returns an empty string for empty input, not FALSE */` |
|      6 | 5272 | `		ph7_result_string(pCtx,"",0);` |
|      6 | 5273 | `		return PH7_OK;` |
|      - | 5274 | `	}` |
|      - | 5275 | `	/* Perform the RFC 3986 URL encoding */` |
|     12 | 5276 | `	SyUriEncodeRaw(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     12 | 5277 | `	return PH7_OK;` |
|      9 | 5278 | `}` |
|      - | 5279 | `/* SyUriEncode/SyUriDecode write through a consumer; both query-string builtins` |
|      - | 5280 | ` * below want the bytes in a blob. */` |
|   5138 | 5281 | `static int UriBlobConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 5282 | `{` |
|   5140 | 5283 | `	return (int)SyBlobAppend((SyBlob *)pUserData,pData,(sxu32)nLen);` |
|      2 | 5284 | `}` |
|      - | 5285 | `/* --- parse_str (php's main/php_variables.c) ---------------------------- */` |
|      - | 5286 |  |
|      - | 5287 | `/*` |
|      - | 5288 | ` * php_register_variable_ex(): register ONE decoded "name[idx][idx]" against a` |
|      - | 5289 | ` * target array. The name arrives ALREADY url-decoded, which is the rule the` |
|      - | 5290 | ` * chunk did not have -- php decodes the whole key first and only then looks for` |
|      - | 5291 | ` * brackets, so "a%5Bb%5D=1" is the NESTED a[b], not a flat key spelled "a[b]".` |
|      - | 5292 | ` *` |
|      - | 5293 | ` * The walk is destructive on its own copy of the name (php writes NULs over the` |
|      - | 5294 | ` * brackets), so zVar must be a writable NUL-terminated buffer.` |
|      - | 5295 | ` */` |
|    346 | 5296 | `static ph7_hashmap * ParseStrDescend(ph7_context *pCtx,ph7_hashmap *pMap,` |
|      - | 5297 | `	const char *zKey,ph7_value *pKey)` |
|      1 | 5298 | `{` |
|    347 | 5299 | `	ph7_hashmap_node *pNode = 0;` |
|      - | 5300 | `	ph7_value *pSlot,*pEmpty;` |
|    347 | 5301 | `	if( zKey ){` |
|    341 | 5302 | `		ph7_value_reset_string_cursor(pKey);` |
|    341 | 5303 | `		ph7_value_string(pKey,zKey,(int)SyStrlen(zKey));` |
|    341 | 5304 | `		if( PH7_HashmapLookup(pMap,pKey,&pNode) == SXRET_OK ){` |
|     27 | 5305 | `			pSlot = HashmapExtractNodeValue(pNode);` |
|     27 | 5306 | `			if( pSlot && (pSlot->iFlags & MEMOBJ_HASHMAP) ){` |
|     23 | 5307 | `				return (ph7_hashmap *)pSlot->x.pOther;` |
|      - | 5308 | `			}` |
|      2 | 5309 | `		}` |
|    159 | 5310 | `	}` |
|      - | 5311 | `	/* Nothing usable there: php OVERWRITES whatever scalar is in the way with a` |
|      - | 5312 | `	 * fresh array ("a=1&a[b]=2" ends as a['b']). */` |
|    325 | 5313 | `	pEmpty = ph7_context_new_array(pCtx);` |
|    325 | 5314 | `	if( pEmpty == 0 \|\| PH7_HashmapInsert(pMap,zKey ? pKey : 0,pEmpty) != SXRET_OK ){` |
|    ! 0 | 5315 | `		return 0;` |
|      - | 5316 | `	}` |
|    325 | 5317 | `	if( zKey ){` |
|    319 | 5318 | `		if( PH7_HashmapLookup(pMap,pKey,&pNode) != SXRET_OK ){` |
|    ! 0 | 5319 | `			return 0;` |
|      - | 5320 | `		}` |
|    160 | 5321 | `	}else{` |
|      7 | 5322 | `		pNode = pMap->pLast;   /* the append just made */` |
|      - | 5323 | `	}` |
|    325 | 5324 | `	pSlot = pNode ? HashmapExtractNodeValue(pNode) : 0;` |
|    325 | 5325 | `	return (pSlot && (pSlot->iFlags & MEMOBJ_HASHMAP)) ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|    174 | 5326 | `}` |
|   2160 | 5327 | `static void ParseStrRegister(ph7_context *pCtx,ph7_value *pTarget,char *zVar,` |
|      - | 5328 | `	ph7_value *pVal,int nMaxNest)` |
|      2 | 5329 | `{` |
|   2162 | 5330 | `	ph7_hashmap *pCur = (ph7_hashmap *)pTarget->x.pOther;` |
|      - | 5331 | `	ph7_value *pIdxKey;` |
|   2162 | 5332 | `	char *p,*ip = 0,*index;` |
|   2162 | 5333 | `	int bIsArray = 0,nNest = 0;` |
|      - | 5334 | `	/* php ignores leading SPACES in the name outright -- they are not mangled to` |
|      - | 5335 | `	 * '_' the way an interior space is. */` |
|   2168 | 5336 | `	while( zVar[0] == ' ' ){` |
|      7 | 5337 | `		zVar++;` |
|      1 | 5338 | `	}` |
|      - | 5339 | `	/* Neither a space nor a dot may live in a php variable name; both become '_'.` |
|      - | 5340 | `	 * The scan stops at the first '[', so only the BASE name is mangled. */` |
|  10218 | 5341 | `	for( p = zVar ; p[0] ; p++ ){` |
|   8138 | 5342 | `		if( p[0] == ' ' \|\| p[0] == '.' ){` |
|     20 | 5343 | `			p[0] = '_';` |
|   8129 | 5344 | `		}else if( p[0] == '[' ){` |
|     81 | 5345 | `			bIsArray = 1;` |
|     81 | 5346 | `			ip = p;` |
|     81 | 5347 | `			p[0] = 0;` |
|     81 | 5348 | `			break;` |
|      - | 5349 | `		}` |
|   4030 | 5350 | `	}` |
|   2162 | 5351 | `	if( p == zVar ){` |
|      5 | 5352 | `		return; /* empty name (or a name that was nothing but a space) */` |
|      - | 5353 | `	}` |
|   2158 | 5354 | `	index = zVar;` |
|   2158 | 5355 | `	pIdxKey = ph7_context_new_scalar(pCtx);` |
|   2158 | 5356 | `	if( pIdxKey == 0 ){` |
|    ! 0 | 5357 | `		return;` |
|      - | 5358 | `	}` |
|   2432 | 5359 | `	while( bIsArray ){` |
|      - | 5360 | `		char *zSeg;` |
|      - | 5361 | `		ph7_hashmap *pNext;` |
|    353 | 5362 | `		if( ++nNest > nMaxNest ){` |
|      - | 5363 | `			/* php drops the whole top-level variable it was building and warns.` |
|      - | 5364 | `			 * The message is deliberately vague about the input -- php calls` |
|      - | 5365 | `			 * saying more "information disclosure". */` |
|      3 | 5366 | `			ph7_hashmap_node *pNode = 0;` |
|      3 | 5367 | `			ph7_hashmap *pRoot = (ph7_hashmap *)pTarget->x.pOther;` |
|      3 | 5368 | `			ph7_value_reset_string_cursor(pIdxKey);` |
|      3 | 5369 | `			ph7_value_string(pIdxKey,zVar,(int)SyStrlen(zVar));` |
|      3 | 5370 | `			if( PH7_HashmapLookup(pRoot,pIdxKey,&pNode) == SXRET_OK ){` |
|      3 | 5371 | `				PH7_HashmapUnlinkNode(pNode,TRUE);` |
|      1 | 5372 | `			}` |
|      4 | 5373 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5374 | `				"Input variable nesting level exceeded %d. To increase the limit "` |
|      1 | 5375 | `				"change max_input_nesting_level in php.ini.",nMaxNest);` |
|      3 | 5376 | `			return;` |
|      - | 5377 | `		}` |
|    351 | 5378 | `		ip++;` |
|    351 | 5379 | `		zSeg = ip;` |
|    351 | 5380 | `		if( ip[0] == ' ' \|\| ip[0] == '\t' \|\| ip[0] == '\n' \|\| ip[0] == '\r' ){` |
|      5 | 5381 | `			ip++;   /* php skips ONE leading space before testing for ']' */` |
|      2 | 5382 | `		}` |
|    351 | 5383 | `		if( ip[0] == ']' ){` |
|     39 | 5384 | `			zSeg = 0;   /* "[]" (and "[ ]") appends */` |
|     20 | 5385 | `		}else{` |
|    651 | 5386 | `			while( ip[0] && ip[0] != ']' ){ ip++; }` |
|    313 | 5387 | `			if( ip[0] == 0 ){` |
|      - | 5388 | `				/* An unterminated '[': php un-terminates the name -- the bracket` |
|      - | 5389 | `				 * itself becomes '_' -- and the rest is mangled and used as a` |
|      - | 5390 | `				 * PLAIN key, so "a[b=1" registers "a_b". */` |
|      5 | 5391 | `				zSeg[-1] = '_';` |
|      7 | 5392 | `				for( p = zSeg ; p[0] ; p++ ){` |
|      3 | 5393 | `					if( p[0] == ' ' \|\| p[0] == '.' \|\| p[0] == '[' ){` |
|    ! 0 | 5394 | `						p[0] = '_';` |
|    ! 0 | 5395 | `					}` |
|      2 | 5396 | `				}` |
|      5 | 5397 | `				break;` |
|      - | 5398 | `			}` |
|    309 | 5399 | `			ip[0] = 0;` |
|      - | 5400 | `		}` |
|    347 | 5401 | `		pNext = ParseStrDescend(pCtx,pCur,index,pIdxKey);` |
|    347 | 5402 | `		if( pNext == 0 ){` |
|    ! 0 | 5403 | `			return;` |
|      - | 5404 | `		}` |
|    347 | 5405 | `		pCur = pNext;` |
|    347 | 5406 | `		index = zSeg;` |
|    347 | 5407 | `		ip++;` |
|    347 | 5408 | `		if( ip[0] == '[' ){` |
|    275 | 5409 | `			ip[0] = 0;   /* another level follows */` |
|    138 | 5410 | `		}else{` |
|     73 | 5411 | `			break;       /* whatever trails the last ']' is ignored */` |
|      - | 5412 | `		}` |
|      1 | 5413 | `	}` |
|   2156 | 5414 | `	if( index == 0 ){` |
|     33 | 5415 | `		PH7_HashmapInsert(pCur,0,pVal);` |
|     17 | 5416 | `	}else{` |
|   2124 | 5417 | `		ph7_value_reset_string_cursor(pIdxKey);` |
|   2124 | 5418 | `		ph7_value_string(pIdxKey,index,(int)SyStrlen(index));` |
|   2124 | 5419 | `		PH7_HashmapInsert(pCur,pIdxKey,pVal);` |
|      - | 5420 | `	}` |
|   1082 | 5421 | `}` |
|      - | 5422 | `/*` |
|      - | 5423 | ` * void parse_str(string $string, array &$result)` |
|      - | 5424 | ` *  Parse a query string into $result the way php's own GET/POST parser does.` |
|      - | 5425 | ` */` |
|    108 | 5426 | `PH7_PRIVATE int PH7_builtin_parse_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5427 | `{` |
|      - | 5428 | `	ph7_value *pArray,*pVal;` |
|      - | 5429 | `	SyBlob sSep,sName,sValue;` |
|      - | 5430 | `	const char *zIn,*zSep;` |
|      - | 5431 | `	int nByte,nSep;` |
|    110 | 5432 | `	sxu32 i = 0;` |
|    110 | 5433 | `	sxi64 nCount = 0,nMaxVars,nMaxNest;` |
|    110 | 5434 | `	if( nArg < 2 ){` |
|      - | 5435 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|    ! 0 | 5436 | `		return PH7_OK;` |
|      - | 5437 | `	}` |
|    110 | 5438 | `	pArray = ph7_context_new_array(pCtx);` |
|    110 | 5439 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    110 | 5440 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 | 5441 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5442 | `	}` |
|    110 | 5443 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|    110 | 5444 | `	nMaxVars = PH7_VmIniGetInt(pCtx->pVm,"max_input_vars",1000);` |
|    110 | 5445 | `	nMaxNest = PH7_VmIniGetInt(pCtx->pVm,"max_input_nesting_level",64);` |
|    110 | 5446 | `	SyBlobInit(&sSep,&pCtx->pVm->sAllocator);` |
|    110 | 5447 | `	SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|    110 | 5448 | `	SyBlobInit(&sValue,&pCtx->pVm->sAllocator);` |
|    110 | 5449 | `	PH7_VmIniGetStr(pCtx->pVm,"arg_separator.input",&sSep);` |
|    110 | 5450 | `	if( SyBlobLength(&sSep) < 1 ){` |
|    ! 0 | 5451 | `		SyBlobAppend(&sSep,"&",sizeof(char));` |
|    ! 0 | 5452 | `	}` |
|    110 | 5453 | `	zSep = (const char *)SyBlobData(&sSep);` |
|    110 | 5454 | `	nSep = (int)SyBlobLength(&sSep);` |
|      - | 5455 | `	/* php tokenizes with strtok(), so the separator is a SET of bytes and a run` |
|      - | 5456 | `	 * of them yields no empty field -- and an embedded NUL ends the input. */` |
|   2270 | 5457 | `	while( i < (sxu32)nByte && zIn[i] ){` |
|      - | 5458 | `		sxu32 iStart,iEq;` |
|      - | 5459 | `		int bFound;` |
|   4230 | 5460 | `		while( i < (sxu32)nByte && zIn[i] ){` |
|      - | 5461 | `			int s;` |
|   6390 | 5462 | `			for( s = 0 ; s < nSep ; ++s ){` |
|   4228 | 5463 | `				if( zIn[i] == zSep[s] ){ break; }` |
|   1083 | 5464 | `			}` |
|   4228 | 5465 | `			if( s == nSep ){ break; }` |
|   2066 | 5466 | `			i++;` |
|      2 | 5467 | `		}` |
|   2166 | 5468 | `		if( i >= (sxu32)nByte \|\| zIn[i] == 0 ){` |
|      2 | 5469 | `			break;` |
|      - | 5470 | `		}` |
|   2164 | 5471 | `		iStart = i;` |
|   2164 | 5472 | `		iEq = 0;` |
|   2164 | 5473 | `		bFound = 0;` |
|  19548 | 5474 | `		while( i < (sxu32)nByte && zIn[i] ){` |
|      - | 5475 | `			int s;` |
|  36832 | 5476 | `			for( s = 0 ; s < nSep ; ++s ){` |
|  19448 | 5477 | `				if( zIn[i] == zSep[s] ){ break; }` |
|   8694 | 5478 | `			}` |
|  19448 | 5479 | `			if( s < nSep ){ break; }` |
|  17386 | 5480 | `			if( zIn[i] == '=' && !bFound ){` |
|   2156 | 5481 | `				iEq = i;` |
|   2156 | 5482 | `				bFound = 1;` |
|   1077 | 5483 | `			}` |
|  17386 | 5484 | `			i++;` |
|      2 | 5485 | `		}` |
|   2164 | 5486 | `		if( ++nCount > nMaxVars ){` |
|      4 | 5487 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5488 | `				"Input variables exceeded %qd. To increase the limit change "` |
|      1 | 5489 | `				"max_input_vars in php.ini.",nMaxVars);` |
|      3 | 5490 | `			break;` |
|      - | 5491 | `		}` |
|      - | 5492 | `		/* Both halves are url-decoded BEFORE the name is parsed for brackets. */` |
|   2162 | 5493 | `		SyBlobReset(&sName);` |
|   2162 | 5494 | `		SyBlobReset(&sValue);` |
|   2162 | 5495 | `		if( bFound ){` |
|   2154 | 5496 | `			if( iEq > iStart ){` |
|   2152 | 5497 | `				SyUriDecode(&zIn[iStart],iEq - iStart,UriBlobConsumer,&sName,TRUE);` |
|   1075 | 5498 | `			}` |
|   2154 | 5499 | `			if( i > iEq + 1 ){` |
|   2152 | 5500 | `				SyUriDecode(&zIn[iEq + 1],i - (iEq + 1),UriBlobConsumer,&sValue,TRUE);` |
|   1075 | 5501 | `			}` |
|   1078 | 5502 | `		}else{` |
|      9 | 5503 | `			SyUriDecode(&zIn[iStart],i - iStart,UriBlobConsumer,&sName,TRUE);` |
|      - | 5504 | `		}` |
|   2162 | 5505 | `		SyBlobAppend(&sName,"\0",sizeof(char));   /* the walk is C-string based */` |
|   2162 | 5506 | `		ph7_value_string(pVal,(const char *)SyBlobData(&sValue),(int)SyBlobLength(&sValue));` |
|   2162 | 5507 | `		ParseStrRegister(pCtx,pArray,(char *)SyBlobData(&sName),pVal,(int)nMaxNest);` |
|   2162 | 5508 | `		ph7_value_reset_string_cursor(pVal);` |
|      2 | 5509 | `	}` |
|    110 | 5510 | `	SyBlobRelease(&sSep);` |
|    110 | 5511 | `	SyBlobRelease(&sName);` |
|    110 | 5512 | `	SyBlobRelease(&sValue);` |
|      - | 5513 | `	/* $result is by REFERENCE and php REPLACES it, empty array included. */` |
|    110 | 5514 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pArray);` |
|    110 | 5515 | `	return PH7_OK;` |
|     56 | 5516 | `}` |
|      - | 5517 | `/* --- http_build_query (php's ext/standard/http.c) ---------------------- */` |
|      - | 5518 |  |
|      - | 5519 | `/*` |
|      - | 5520 | ` * The chain of hashmaps and instances the walk is currently INSIDE. This is` |
|      - | 5521 | ` * php's GC_TRY_PROTECT_RECURSION without a mark bit: a container that is its own` |
|      - | 5522 | `` * ancestor contributes nothing, so `$a['self'] = &$a` builds "a=1" rather than`` |
|      - | 5523 | ` * recursing forever. PHL had no guard here at all and ran the allocator out of` |
|      - | 5524 | ` * memory on exactly that input.` |
|      - | 5525 | ` */` |
|      - | 5526 | `typedef struct http_query_frame http_query_frame;` |
|      - | 5527 | `struct http_query_frame {` |
|      - | 5528 | `	const void *pWalked;                  /* the ph7_hashmap / ph7_class_instance */` |
|      - | 5529 | `	const http_query_frame *pParent;` |
|      - | 5530 | `};` |
|      - | 5531 | `typedef struct http_query_state http_query_state;` |
|      - | 5532 | `struct http_query_state {` |
|      - | 5533 | `	ph7_context *pCtx;` |
|      - | 5534 | `	SyBlob *pOut;      /* the form string built so far */` |
|      - | 5535 | `	const char *zSep;  /* argument separator */` |
|      - | 5536 | `	sxu32 nSep;` |
|      - | 5537 | `	int bRaw;          /* PHP_QUERY_RFC3986 rather than RFC1738 */` |
|      - | 5538 | `	int nDepth;` |
|      - | 5539 | `	int rc;            /* PH7_OK, or the status of a throw in flight */` |
|      - | 5540 | `};` |
|      - | 5541 | `/*` |
|      - | 5542 | ` * php has no fixed nesting limit here -- it asks the platform whether the C` |
|      - | 5543 | ` * stack is nearly gone and throws "Maximum call stack size reached." when it is.` |
|      - | 5544 | ` * PHL walks the same tree on the same C stack, so it needs a bound; this one is` |
|      - | 5545 | ` * far above any query string anyone builds and reports php's own error.` |
|      - | 5546 | ` */` |
|      - | 5547 | `#define HTTP_QUERY_MAX_DEPTH 512` |
|      - | 5548 |  |
|    696 | 5549 | `static int HttpQueryIsAncestor(const http_query_frame *pFrame,const void *pWalked)` |
|      1 | 5550 | `{` |
|  90431 | 5551 | `	while( pFrame ){` |
|  89739 | 5552 | `		if( pFrame->pWalked == pWalked ){` |
|      5 | 5553 | `			return 1;` |
|      - | 5554 | `		}` |
|  89735 | 5555 | `		pFrame = pFrame->pParent;` |
|      1 | 5556 | `	}` |
|    693 | 5557 | `	return 0;` |
|    349 | 5558 | `}` |
|    780 | 5559 | `static void HttpQueryEncodeTo(SyBlob *pOut,int bRaw,const char *zIn,sxu32 nByte)` |
|      1 | 5560 | `{` |
|    781 | 5561 | `	if( nByte < 1 ){` |
|    ! 0 | 5562 | `		return;` |
|      - | 5563 | `	}` |
|    781 | 5564 | `	if( bRaw ){` |
|      5 | 5565 | `		SyUriEncodeRaw(zIn,nByte,UriBlobConsumer,pOut);` |
|      3 | 5566 | `	}else{` |
|    777 | 5567 | `		SyUriEncode(zIn,nByte,UriBlobConsumer,pOut);` |
|      - | 5568 | `	}` |
|    391 | 5569 | `}` |
|      - | 5570 | `static int HttpQueryWalk(http_query_state *p,ph7_value *pData,` |
|      - | 5571 | `	const char *zNumPrefix,sxu32 nNumPrefix,` |
|      - | 5572 | `	const char *zKeyPrefix,sxu32 nKeyPrefix,` |
|      - | 5573 | `	const http_query_frame *pParent);` |
|      - | 5574 |  |
|      - | 5575 | `/*` |
|      - | 5576 | ` * php_url_encode_scalar(): one "<key_prefix><key>[%5D]=<value>" leaf, preceded` |
|      - | 5577 | ` * by the separator once anything has been written.` |
|      - | 5578 | ` */` |
|    106 | 5579 | `static void HttpQueryScalar(http_query_state *p,` |
|      - | 5580 | `	int bIntKey,sxi64 iKey,const char *zKey,sxu32 nKey,` |
|      - | 5581 | `	ph7_value *pVal,` |
|      - | 5582 | `	const char *zNumPrefix,sxu32 nNumPrefix,` |
|      - | 5583 | `	const char *zKeyPrefix,sxu32 nKeyPrefix)` |
|      1 | 5584 | `{` |
|    107 | 5585 | `	if( SyBlobLength(p->pOut) > 0 ){` |
|     49 | 5586 | `		SyBlobAppend(p->pOut,p->zSep,p->nSep);` |
|     24 | 5587 | `	}` |
|    107 | 5588 | `	if( nKeyPrefix > 0 ){` |
|     45 | 5589 | `		SyBlobAppend(p->pOut,zKeyPrefix,nKeyPrefix);` |
|     22 | 5590 | `	}` |
|    107 | 5591 | `	if( bIntKey ){` |
|      - | 5592 | `		/* The numeric prefix is appended RAW -- php never url-encodes it, which` |
|      - | 5593 | `		 * is why http_build_query([1,2], "a b") answers "a b0=1&a b1=2". The` |
|      - | 5594 | `		 * chunk encoded it and answered "a+b0=1". */` |
|     53 | 5595 | `		if( nNumPrefix > 0 ){` |
|     17 | 5596 | `			SyBlobAppend(p->pOut,zNumPrefix,nNumPrefix);` |
|      8 | 5597 | `		}` |
|     53 | 5598 | `		SyBlobFormat(p->pOut,"%qd",iKey);` |
|     27 | 5599 | `	}else{` |
|     55 | 5600 | `		HttpQueryEncodeTo(p->pOut,p->bRaw,zKey,nKey);` |
|      - | 5601 | `	}` |
|    107 | 5602 | `	if( nKeyPrefix > 0 ){` |
|     45 | 5603 | `		SyBlobAppend(p->pOut,"%5D",sizeof("%5D")-1);` |
|     22 | 5604 | `	}` |
|    107 | 5605 | `	SyBlobAppend(p->pOut,"=",sizeof(char));` |
|    107 | 5606 | `	if( ph7_value_is_bool(pVal) ){` |
|      - | 5607 | `		/* php writes the digit itself: to_string() would give "" for false. */` |
|      5 | 5608 | `		SyBlobAppend(p->pOut,ph7_value_to_bool(pVal) ? "1" : "0",sizeof(char));` |
|      3 | 5609 | `	}else{` |
|      - | 5610 | `		int nVal;` |
|    103 | 5611 | `		const char *zVal = ph7_value_to_string(pVal,&nVal);` |
|    103 | 5612 | `		HttpQueryEncodeTo(p->pOut,p->bRaw,zVal,(sxu32)nVal);` |
|      - | 5613 | `	}` |
|    107 | 5614 | `}` |
|      - | 5615 | `/*` |
|      - | 5616 | ` * Build the key prefix a nested container's members carry: php closes the` |
|      - | 5617 | ` * PREVIOUS bracket and opens the next one in the same step, so a second level` |
|      - | 5618 | ` * appends "%5D%5B" where the first opened with "%5B".` |
|      - | 5619 | ` */` |
|    634 | 5620 | `static void HttpQueryNestPrefix(http_query_state *p,SyBlob *pPrefix,` |
|      - | 5621 | `	int bIntKey,sxi64 iKey,const char *zKey,sxu32 nKey,` |
|      - | 5622 | `	const char *zNumPrefix,sxu32 nNumPrefix,` |
|      - | 5623 | `	const char *zKeyPrefix,sxu32 nKeyPrefix)` |
|      1 | 5624 | `{` |
|    635 | 5625 | `	if( nKeyPrefix > 0 ){` |
|    601 | 5626 | `		SyBlobAppend(pPrefix,zKeyPrefix,nKeyPrefix);` |
|    335 | 5627 | `	}else if( bIntKey && nNumPrefix > 0 ){` |
|      9 | 5628 | `		SyBlobAppend(pPrefix,zNumPrefix,nNumPrefix);` |
|      4 | 5629 | `	}` |
|    635 | 5630 | `	if( bIntKey ){` |
|     11 | 5631 | `		SyBlobFormat(pPrefix,"%qd",iKey);` |
|      6 | 5632 | `	}else{` |
|    625 | 5633 | `		HttpQueryEncodeTo(pPrefix,p->bRaw,zKey,nKey);` |
|      - | 5634 | `	}` |
|    952 | 5635 | `	SyBlobAppend(pPrefix,nKeyPrefix > 0 ? "%5D%5B" : "%5B",` |
|    317 | 5636 | `		nKeyPrefix > 0 ? sizeof("%5D%5B")-1 : sizeof("%5B")-1);` |
|    635 | 5637 | `}` |
|      - | 5638 | `/*` |
|      - | 5639 | ` * One (key, value) pair, whichever container it came from. php skips NULL and` |
|      - | 5640 | ` * RESOURCE outright, descends into an array or a non-enum object, and treats` |
|      - | 5641 | ` * everything else -- a backed enum case included -- as a scalar.` |
|      - | 5642 | ` */` |
|    748 | 5643 | `static void HttpQueryPair(http_query_state *p,` |
|      - | 5644 | `	int bIntKey,sxi64 iKey,const char *zKey,sxu32 nKey,` |
|      - | 5645 | `	ph7_value *pVal,` |
|      - | 5646 | `	const char *zNumPrefix,sxu32 nNumPrefix,` |
|      - | 5647 | `	const char *zKeyPrefix,sxu32 nKeyPrefix,` |
|      - | 5648 | `	const http_query_frame *pParent)` |
|      1 | 5649 | `{` |
|      - | 5650 | `	int bDescend;` |
|    749 | 5651 | `	if( p->rc != PH7_OK ){` |
|    ! 0 | 5652 | `		return;` |
|      - | 5653 | `	}` |
|    749 | 5654 | `	if( ph7_value_is_null(pVal) \|\| ph7_value_is_resource(pVal) ){` |
|      7 | 5655 | `		return;` |
|      - | 5656 | `	}` |
|    743 | 5657 | `	bDescend = ph7_value_is_array(pVal);` |
|    743 | 5658 | `	if( ph7_value_is_object(pVal) ){` |
|     11 | 5659 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|     11 | 5660 | `		if( (pInst->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      5 | 5661 | `			bDescend = 1;` |
|      3 | 5662 | `		}else{` |
|      - | 5663 | `			/* php compares an enum case by its BACKING value here; the chunk` |
|      - | 5664 | `			 * descended into it and emitted its name/value properties. */` |
|      7 | 5665 | `			ph7_value *pBacking = PH7_EnumCaseBackingValueOf(pInst);` |
|      7 | 5666 | `			if( pBacking == 0 ){` |
|      5 | 5667 | `				p->rc = PH7_VmThrowException(p->pCtx,"ValueError",` |
|      - | 5668 | `					"Unbacked enum %z cannot be converted to a string",` |
|      2 | 5669 | `					&pInst->pClass->sDisp);` |
|      3 | 5670 | `				return;` |
|      - | 5671 | `			}` |
|      7 | 5672 | `			HttpQueryScalar(p,bIntKey,iKey,zKey,nKey,pBacking,` |
|      2 | 5673 | `				zNumPrefix,nNumPrefix,zKeyPrefix,nKeyPrefix);` |
|      5 | 5674 | `			return;` |
|      - | 5675 | `		}` |
|      2 | 5676 | `	}` |
|    737 | 5677 | `	if( bDescend ){` |
|      - | 5678 | `		SyBlob sPrefix;` |
|    635 | 5679 | `		SyBlobInit(&sPrefix,&p->pCtx->pVm->sAllocator);` |
|    952 | 5680 | `		HttpQueryNestPrefix(p,&sPrefix,bIntKey,iKey,zKey,nKey,` |
|    317 | 5681 | `			zNumPrefix,nNumPrefix,zKeyPrefix,nKeyPrefix);` |
|      - | 5682 | `		/* php passes no numeric prefix down: it only ever prefixes a TOP-LEVEL` |
|      - | 5683 | `		 * integer key. */` |
|    952 | 5684 | `		HttpQueryWalk(p,pVal,0,0,` |
|    634 | 5685 | `			(const char *)SyBlobData(&sPrefix),SyBlobLength(&sPrefix),pParent);` |
|    635 | 5686 | `		SyBlobRelease(&sPrefix);` |
|    635 | 5687 | `		return;` |
|      - | 5688 | `	}` |
|    154 | 5689 | `	HttpQueryScalar(p,bIntKey,iKey,zKey,nKey,pVal,` |
|     51 | 5690 | `		zNumPrefix,nNumPrefix,zKeyPrefix,nKeyPrefix);` |
|    375 | 5691 | `}` |
|      - | 5692 | `/* Every visible, non-static, materialized property of an instance, in` |
|      - | 5693 | ` * declaration order. php asks the CALLER's scope, so http_build_query($this)` |
|      - | 5694 | ` * from inside the class sees its private members -- the chunk reached them` |
|      - | 5695 | ` * through a global-scope get_object_vars() and never did. */` |
|      8 | 5696 | `static void HttpQueryWalkObject(http_query_state *p,ph7_class_instance *pThis,` |
|      - | 5697 | `	const char *zKeyPrefix,sxu32 nKeyPrefix,const http_query_frame *pFrame)` |
|      1 | 5698 | `{` |
|      - | 5699 | `	SyHashEntry *pEntry;` |
|      - | 5700 | `	ph7_value sValue;` |
|      9 | 5701 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|      9 | 5702 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     39 | 5703 | `	while( p->rc == PH7_OK && (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     31 | 5704 | `		VmClassAttr *pAttr = (VmClassAttr *)pEntry->pUserData;` |
|     31 | 5705 | `		SyString *pName = &pAttr->pAttr->sName;` |
|      - | 5706 | `		ph7_value *pValue;` |
|     31 | 5707 | `		if( PH7_ATTR_UNPRESENTED(pAttr) ){` |
|    ! 0 | 5708 | `			continue;` |
|      - | 5709 | `		}` |
|     31 | 5710 | `		if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|      - | 5711 | `			/* A virtual hooked property has no backing store, and php reads the` |
|      - | 5712 | `			 * raw property table here rather than dispatching the get hook. */` |
|      5 | 5713 | `			continue;` |
|      - | 5714 | `		}` |
|     40 | 5715 | `		if( !PH7_VmClassMemberAccess(pThis->pVm,pThis->pClass,pName,` |
|     26 | 5716 | `				pAttr->pAttr->iProtection,FALSE) ){` |
|      7 | 5717 | `			continue;` |
|      - | 5718 | `		}` |
|     21 | 5719 | `		pValue = PH7_ClassInstanceExtractAttrValue(pThis,pAttr);` |
|     21 | 5720 | `		if( pValue == 0 ){` |
|    ! 0 | 5721 | `			continue;` |
|      - | 5722 | `		}` |
|     21 | 5723 | `		PH7_MemObjLoad(pValue,&sValue);` |
|     31 | 5724 | `		HttpQueryPair(p,0,0,SyStringData(pName),SyStringLength(pName),&sValue,` |
|     10 | 5725 | `			0,0,zKeyPrefix,nKeyPrefix,pFrame);` |
|     21 | 5726 | `		PH7_MemObjRelease(&sValue);` |
|      1 | 5727 | `	}` |
|      9 | 5728 | `	PH7_MemObjRelease(&sValue);` |
|      9 | 5729 | `}` |
|      - | 5730 | `/* php_url_encode_hash_ex() over one array or object. */` |
|    696 | 5731 | `static int HttpQueryWalk(http_query_state *p,ph7_value *pData,` |
|      - | 5732 | `	const char *zNumPrefix,sxu32 nNumPrefix,` |
|      - | 5733 | `	const char *zKeyPrefix,sxu32 nKeyPrefix,` |
|      - | 5734 | `	const http_query_frame *pParent)` |
|      1 | 5735 | `{` |
|      - | 5736 | `	http_query_frame sFrame;` |
|    697 | 5737 | `	const void *pWalked = pData->x.pOther;` |
|    697 | 5738 | `	if( HttpQueryIsAncestor(pParent,pWalked) ){` |
|      5 | 5739 | `		return PH7_OK;` |
|      - | 5740 | `	}` |
|    693 | 5741 | `	if( p->nDepth >= HTTP_QUERY_MAX_DEPTH ){` |
|    ! 0 | 5742 | `		p->rc = PH7_VmThrowException(p->pCtx,"Error","Maximum call stack size reached.");` |
|    ! 0 | 5743 | `		return p->rc;` |
|      - | 5744 | `	}` |
|    693 | 5745 | `	sFrame.pWalked = pWalked;` |
|    693 | 5746 | `	sFrame.pParent = pParent;` |
|    693 | 5747 | `	p->nDepth++;` |
|    693 | 5748 | `	if( ph7_value_is_object(pData) ){` |
|      9 | 5749 | `		HttpQueryWalkObject(p,(ph7_class_instance *)pWalked,zKeyPrefix,nKeyPrefix,&sFrame);` |
|      5 | 5750 | `	}else{` |
|    685 | 5751 | `		ph7_hashmap *pMap = (ph7_hashmap *)pWalked;` |
|    685 | 5752 | `		ph7_hashmap_node *pNode = pMap->pFirst;` |
|      - | 5753 | `		ph7_value sValue;` |
|    685 | 5754 | `		sxu32 n = pMap->nEntry;` |
|    685 | 5755 | `		PH7_MemObjInit(pMap->pVm,&sValue);` |
|      - | 5756 | `		/* Insertion order runs pFirst then the pPrev chain (MACRO_LD_PUSH links` |
|      - | 5757 | `		 * a new node in through pNext, so pNext is the OLDER neighbour). */` |
|   1413 | 5758 | `		while( n > 0 && p->rc == PH7_OK ){` |
|    729 | 5759 | `			int bIntKey = (pNode->iType == HASHMAP_INT_NODE);` |
|    729 | 5760 | `			PH7_HashmapExtractNodeValue(pNode,&sValue,FALSE);` |
|   1093 | 5761 | `			HttpQueryPair(p,bIntKey,bIntKey ? pNode->xKey.iKey : 0,` |
|    364 | 5762 | `				bIntKey ? 0 : (const char *)SyBlobData(&pNode->xKey.sKey),` |
|    364 | 5763 | `				bIntKey ? 0 : SyBlobLength(&pNode->xKey.sKey),` |
|    364 | 5764 | `				&sValue,zNumPrefix,nNumPrefix,zKeyPrefix,nKeyPrefix,&sFrame);` |
|    729 | 5765 | `			PH7_MemObjRelease(&sValue);` |
|    729 | 5766 | `			pNode = pNode->pPrev;` |
|    729 | 5767 | `			n--;` |
|      1 | 5768 | `		}` |
|    685 | 5769 | `		PH7_MemObjRelease(&sValue);` |
|      - | 5770 | `	}` |
|    693 | 5771 | `	p->nDepth--;` |
|    693 | 5772 | `	return p->rc;` |
|    349 | 5773 | `}` |
|      - | 5774 | `/*` |
|      - | 5775 | ` * string http_build_query(object\|array $data, string $numeric_prefix = "",` |
|      - | 5776 | ` *                         ?string $arg_separator = null,` |
|      - | 5777 | ` *                         int $encoding_type = PHP_QUERY_RFC1738)` |
|      - | 5778 | ` *  Generate a URL-encoded query string from an array or an object.` |
|      - | 5779 | ` */` |
|     76 | 5780 | `PH7_PRIVATE int PH7_builtin_http_build_query(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5781 | `{` |
|      - | 5782 | `	http_query_state sState;` |
|      - | 5783 | `	SyBlob sOut;` |
|      - | 5784 | `	char zName[64];` |
|     78 | 5785 | `	const char *zNumPrefix = 0,*zSep = "&";` |
|     78 | 5786 | `	int nNumPrefix = 0,nSep = 1;` |
|     78 | 5787 | `	if( nArg < 1 ){` |
|      - | 5788 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|    ! 0 | 5789 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5790 | `		return PH7_OK;` |
|      - | 5791 | `	}` |
|      - | 5792 | ``	/* php DECLARES `object\|array $data` and REPORTS "must be of type array" --`` |
|      - | 5793 | ``	 * the shared ZPP screen leaves a union arm holding `array` alone for exactly`` |
|      - | 5794 | `	 * this reason, so the wording is the builtin's own. */` |
|     78 | 5795 | `	if( !ph7_value_is_array(apArg[0]) && !ph7_value_is_object(apArg[0]) ){` |
|     16 | 5796 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5797 | `			"http_build_query(): Argument #1 ($data) must be of type array, %s given",` |
|      5 | 5798 | `			VmValueGivenName(apArg[0],zName,sizeof(zName)));` |
|      - | 5799 | `	}` |
|     67 | 5800 | `	if( ph7_value_is_object(apArg[0]) ){` |
|     11 | 5801 | `		ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     11 | 5802 | `		if( pInst->pClass->iFlags & PH7_CLASS_ENUM ){` |
|      7 | 5803 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5804 | `				"http_build_query(): Argument #1 ($data) must not be an enum, %z given",` |
|      4 | 5805 | `				&pInst->pClass->sDisp);` |
|      - | 5806 | `		}` |
|      3 | 5807 | `	}` |
|     63 | 5808 | `	if( nArg > 1 ){` |
|     33 | 5809 | `		zNumPrefix = ph7_value_to_string(apArg[1],&nNumPrefix);` |
|     16 | 5810 | `	}` |
|     63 | 5811 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      3 | 5812 | `		zSep = ph7_value_to_string(apArg[2],&nSep);` |
|      1 | 5813 | `	}` |
|     63 | 5814 | `	sState.pCtx = pCtx;` |
|     63 | 5815 | `	sState.zSep = zSep;` |
|     63 | 5816 | `	sState.nSep = (sxu32)nSep;` |
|     63 | 5817 | `	sState.bRaw = (nArg > 3) && (ph7_value_to_int(apArg[3]) == 2 /* PHP_QUERY_RFC3986 */);` |
|     63 | 5818 | `	sState.nDepth = 0;` |
|     63 | 5819 | `	sState.rc = PH7_OK;` |
|     63 | 5820 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     63 | 5821 | `	sState.pOut = &sOut;` |
|     63 | 5822 | `	HttpQueryWalk(&sState,apArg[0],zNumPrefix,(sxu32)nNumPrefix,0,0,0);` |
|     63 | 5823 | `	if( sState.rc == PH7_OK ){` |
|     61 | 5824 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     30 | 5825 | `	}` |
|     63 | 5826 | `	SyBlobRelease(&sOut);` |
|     63 | 5827 | `	return sState.rc;` |
|     40 | 5828 | `}` |
|      - | 5829 | `/*` |
|      - | 5830 | ` * string urldecode(string $str)` |
|      - | 5831 | ` *  Decodes any %## encoding in the given string.` |
|      - | 5832 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 5833 | ` * string rawurldecode(string $str)` |
|      - | 5834 | ` *  The same, except that '+' is NOT a space: RFC 3986 has no plus convention, so` |
|      - | 5835 | ` *  php leaves it alone. rawurldecode() used to be registered as an ALIAS of` |
|      - | 5836 | ` *  urldecode(), which turned every literal '+' into a space.` |
|      - | 5837 | ` * Parameter` |
|      - | 5838 | ` *  $data` |
|      - | 5839 | ` *    Input string.` |
|      - | 5840 | ` * Return` |
|      - | 5841 | ` *  Decoded URL or FALSE on failure.` |
|      - | 5842 | ` */` |
|    120 | 5843 | `static int UrlDecodeCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPlus)` |
|      2 | 5844 | `{` |
|      - | 5845 | `	const char *zIn;` |
|      - | 5846 | `	int nLen;` |
|    122 | 5847 | `	if( nArg < 1 ){` |
|      - | 5848 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 5849 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5850 | `		return PH7_OK;` |
|      - | 5851 | `	}` |
|      - | 5852 | `	/* Extract the input string */` |
|    122 | 5853 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    122 | 5854 | `	if( nLen < 1 ){` |
|      - | 5855 | `		/* php returns an empty string for empty input, not FALSE */` |
|     12 | 5856 | `		ph7_result_string(pCtx,"",0);` |
|     12 | 5857 | `		return PH7_OK;` |
|      - | 5858 | `	}` |
|      - | 5859 | `	/* Perform the URL decoding */` |
|    112 | 5860 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,bPlus);` |
|    112 | 5861 | `	return PH7_OK;` |
|     62 | 5862 | `}` |
|     64 | 5863 | `PH7_PRIVATE int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5864 | `{` |
|     66 | 5865 | `	return UrlDecodeCommon(pCtx,nArg,apArg,TRUE);` |
|      2 | 5866 | `}` |
|     56 | 5867 | `PH7_PRIVATE int PH7_builtin_rawurldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5868 | `{` |
|     58 | 5869 | `	return UrlDecodeCommon(pCtx,nArg,apArg,FALSE);` |
|      2 | 5870 | `}` |
|      - | 5871 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5872 |  |
