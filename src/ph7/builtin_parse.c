/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <stdlib.h>  /* strtod */
#include <math.h>    /* HUGE_VAL */
#include <errno.h>   /* ERANGE (strtod range-error signal) */
/*
 * Section:
 *    Parsing/classification functions: filter_var, CSV, strip_tags,
 *    parse_ini_string, the ctype_* family and URL/base64 coding.
 * Status:
 *    Stable.
 */
#ifndef PH7_DISABLE_BUILTIN_FUNC
#define PH7_NEED_BUILTIN_REG 1
#endif
#ifndef PH7_DISABLE_DISK_IO
#define PH7_NEED_FMT_AND_INI 1
#endif
#ifdef PH7_NEED_BUILTIN_REG
/*
 * filter_var() — input validation and sanitization (the ext/filter API).
 *
 * Filter and flag identifiers (values match PHP 8.5; the constants themselves
 * are registered in constant.c). The validate filters are hand-rolled rather
 * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading
 * zeros and cannot signal overflow, and the latter treats ',' as a decimal point
 * unconditionally — neither matches PHP's filter semantics.
 */
#define FV_VALIDATE_INT     257
#define FV_VALIDATE_BOOLEAN 258
#define FV_VALIDATE_FLOAT   259
#define FV_VALIDATE_REGEXP  272
#define FV_VALIDATE_URL     273
#define FV_VALIDATE_EMAIL   274
#define FV_VALIDATE_IP      275
#define FV_VALIDATE_MAC     276
#define FV_VALIDATE_DOMAIN  277
#define FV_SANITIZE_SPECIAL_CHARS      515
#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */
#define FV_SANITIZE_EMAIL   517
#define FV_SANITIZE_URL     518
#define FV_SANITIZE_NUMBER_INT   519
#define FV_SANITIZE_NUMBER_FLOAT 520
#define FV_SANITIZE_FULL_SPECIAL_CHARS 522
#define FV_FLAG_ALLOW_OCTAL  1
#define FV_FLAG_ALLOW_HEX    2
#define FV_FLAG_STRIP_LOW    4
#define FV_FLAG_STRIP_HIGH   8
#define FV_FLAG_ENCODE_LOW   16
#define FV_FLAG_ENCODE_HIGH  32
#define FV_FLAG_ENCODE_AMP   64
#define FV_FLAG_NO_ENCODE_QUOTES 128
#define FV_FLAG_STRIP_BACKTICK   512
#define FV_FLAG_ALLOW_FRACTION   4096
#define FV_FLAG_ALLOW_THOUSAND   8192
#define FV_FLAG_ALLOW_SCIENTIFIC 16384
#define FV_FLAG_IPV4  1048576
#define FV_FLAG_IPV6  2097152
#define FV_NULL_ON_FAILURE 134217728
/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)
 * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT
 * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */
#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW|FV_FLAG_STRIP_HIGH|FV_FLAG_STRIP_BACKTICK \
                            |FV_FLAG_ENCODE_LOW|FV_FLAG_ENCODE_HIGH|FV_FLAG_ENCODE_AMP)

/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.
 * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */
static void FvTrim(const char **pz,int *pn){
	const char *z = *pz;
	int n = *pn;
	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }
	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }
	*pz = z; *pn = n;
}
/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */
static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){
	int neg = 0, i;
	sxu64 u = 0;
	FvTrim(&z,&n);
	if( n==0 ){ return 0; }
	if( z[0]=='+' || z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }
	if( n==0 ){ return 0; }
	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'||z[1]=='X') ){
		z += 2; n -= 2;
		if( n==0 ){ return 0; }
		for( i=0; i<n; i++ ){
			int h = SyHexToint((unsigned char)z[i]);
			if( h<0 ){ return 0; }
			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }
			u = u*16 + (sxu64)h;
		}
	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){
		for( i=0; i<n; i++ ){
			if( z[i]<'0' || z[i]>'7' ){ return 0; }
			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }
			u = u*8 + (sxu64)(z[i]-'0');
		}
	}else{
		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */
		for( i=0; i<n; i++ ){
			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }
			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }
			u = u*10 + (sxu64)(z[i]-'0');
		}
	}
	if( neg ){
		if( u > 0x8000000000000000ULL ){ return 0; }
		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */
	}else{
		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }
		*pOut = (ph7_int64)u;
	}
	return 1;
}
/* Is byte c one of the nSep thousand separators in zSep? */
static int FvIsThousandSep(const char *zSep,int nSep,int c){
	int i;
	for( i=0; i<nSep; i++ ){
		if( (unsigned char)zSep[i] == (unsigned char)c ){ return 1; }
	}
	return 0;
}
/*
 * FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure.
 *
 * decSep is the byte that separates the fractional part (php's "decimal" option,
 * '.' by default) and zSep[0..nSep) the set that may group the INTEGER part when
 * FILTER_FLAG_ALLOW_THOUSAND is set (php's "thousand" option, "',." by default).
 * The two sets overlap by default, and php tests the decimal separator FIRST —
 * which is why `decimal => ','` leaves '.' working as a group separator.
 *
 * The number is normalized into zBuf as a plain C double literal (separators
 * dropped, decSep rewritten to '.') and handed to strtod.
 */
static int FvValidateFloat(const char *z,int n,int flags,int decSep,
                           const char *zSep,int nSep,double *pOut){
	/* decSep is a BYTE value (0..255): a separator above 127 — php takes any
	 * single byte, including one out of a UTF-8 sequence — must not be compared
	 * against a sign-extended char. */
	char zBuf[512];
	int i = 0, m = 0, seenDigit = 0, grouped = 0, nGroup = 0, runLen;
	int hasExp = 0, expNonZero = 0, hasDot = 0;
	double d = 0;
	FvTrim(&z,&n);
	/* Bound the input: zBuf[512] holds the separator-stripped copy, and the cap
	 * also rejects the pathological 500+ digit floats PHP refuses. */
	if( n==0 || n>500 ){ return 0; }
	if( i<n && (z[i]=='+'||z[i]=='-') ){ zBuf[m++] = z[i]; i++; }
	/* The integer part: digit runs, optionally separated by a thousand separator.
	 * A separator anywhere means the runs must GROUP — a leading run of 1..3
	 * digits then runs of exactly 3 ("1,000" and "1'234,567" ok, "1,5" and
	 * "1234,567" rejected); with no separator the run is any length at all
	 * (including zero, which is how ".5" parses). */
	for(;;){
		runLen = 0;
		while( i<n && SyisDigit((unsigned char)z[i]) ){ zBuf[m++] = z[i]; i++; runLen++; }
		if( runLen>0 ){ seenDigit = 1; }
		if( i<n && (unsigned char)z[i]!=decSep && (flags & FV_FLAG_ALLOW_THOUSAND)
		 && FvIsThousandSep(zSep,nSep,z[i]) ){
			if( nGroup==0 ){ if( runLen<1 || runLen>3 ){ return 0; } }
			else if( runLen!=3 ){ return 0; }
			grouped = 1; nGroup++;
			i++;                       /* drop the separator itself */
			continue;
		}
		if( grouped && runLen!=3 ){ return 0; } /* the run that closes a grouped number */
		break;
	}
	if( i<n && (unsigned char)z[i]==decSep ){
		zBuf[m++] = '.';
		hasDot = 1;
		i++;
		while( i<n && SyisDigit((unsigned char)z[i]) ){ zBuf[m++] = z[i]; i++; seenDigit = 1; }
	}
	if( !seenDigit ){ return 0; }
	if( i<n && (z[i]=='e'||z[i]=='E') ){
		zBuf[m++] = z[i];
		i++;
		if( i<n && (z[i]=='+'||z[i]=='-') ){ zBuf[m++] = z[i]; i++; }
		if( i>=n || !SyisDigit((unsigned char)z[i]) ){ return 0; }
		while( i<n && SyisDigit((unsigned char)z[i]) ){
			if( z[i]!='0' ){ expNonZero = 1; }
			zBuf[m++] = z[i]; i++;
		}
		hasExp = 1;
	}
	if( i!=n ){ return 0; } /* trailing junk */
	/* The grammar above guarantees zBuf[0..m) is a clean ASCII decimal float (no hex /
	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike
	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates
	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and
	 * correctly rounded. Every byte written to zBuf consumed one input byte, so
	 * m <= n <= 500 < sizeof(zBuf) and the NUL below is in range.
	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow
	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */
	zBuf[m] = 0;
	errno = 0;
	d = strtod(zBuf,0);
	if( errno == ERANGE && (d == HUGE_VAL || d == -HUGE_VAL || d == 0.0) ){
		return 0;
	}
	/* php's own strtod reports a zero answer carrying a non-zero EXPONENT as an
	 * underflow, where glibc's leaves errno alone: "0e1", "0.e5" and ".0e5" are
	 * refused while "0", "0.0" and "0e0" are the float zero. */
	if( d == 0.0 && hasExp && expNonZero ){ return 0; }
	/* An INTEGER-shaped literal is answered through php's long path, which has no
	 * signed zero: "-0" is the float +0.0 where "-0.0" and "-0e0" stay negative. */
	if( d == 0.0 && !hasDot && !hasExp ){ d = 0.0; }
	*pOut = d;
	return 1;
}
/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),
 * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as
 * false, NOT failures. */
static int FvValidateBool(const char *z,int n,int *pBool){
	FvTrim(&z,&n);
	if( (n==1 && z[0]=='1') || (n==4 && SyStrnicmp(z,"true",4)==0)
	    || (n==2 && SyStrnicmp(z,"on",2)==0) || (n==3 && SyStrnicmp(z,"yes",3)==0) ){
		*pBool = 1; return 1;
	}
	if( n==0 || (n==1 && z[0]=='0') || (n==5 && SyStrnicmp(z,"false",5)==0)
	    || (n==3 && SyStrnicmp(z,"off",3)==0) || (n==2 && SyStrnicmp(z,"no",2)==0) ){
		*pBool = 0; return 1;
	}
	return 0;
}
/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */
static int FvValidateIp4(const char *z,int n){
	int i = 0, parts = 0;
	while( i<n ){
		int val = 0, digits = 0, start = i;
		while( i<n && SyisDigit((unsigned char)z[i]) ){
			val = val*10 + (z[i]-'0');
			if( val>255 ){ return 0; }
			digits++; i++;
		}
		if( digits==0 || digits>3 ){ return 0; }
		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */
		parts++;
		if( parts>4 ){ return 0; }
		if( i<n ){
			if( z[i]!='.' ){ return 0; }
			i++;
			if( i>=n ){ return 0; } /* trailing dot */
		}
	}
	return parts==4;
}
/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),
 * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */
static int FvIp6Hextets(const char *z,int n){
	int i = 0, segStart = 0, groups = 0;
	if( n==0 ){ return 0; }
	while( i<=n ){
		if( i==n || z[i]==':' ){
			int segLen = i - segStart, j, isV4 = 0;
			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */
			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }
			if( isV4 ){
				if( i!=n ){ return -1; } /* IPv4 only as the final token */
				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }
				groups += 2;
			}else{
				if( segLen>4 ){ return -1; }
				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }
				groups++;
			}
			segStart = i+1;
		}
		i++;
	}
	return groups;
}
/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */
static int FvValidateIp6(const char *z,int n){
	const char *zDbl = 0;
	int i, ga, gb;
	for( i=0; i+1<n; i++ ){
		if( z[i]==':' && z[i+1]==':' ){
			if( zDbl ){ return 0; } /* a second "::" is invalid */
			zDbl = z+i;
		}
	}
	if( zDbl==0 ){
		return FvIp6Hextets(z,n)==8;
	}else{
		int lenA = (int)(zDbl - z);
		int lenB = n - lenA - 2;
		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);
		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);
		if( ga<0 || gb<0 ){ return 0; }
		return (ga+gb)<=7; /* "::" stands for at least one zero group */
	}
}
static int FvValidateIp(const char *z,int n,int flags){
	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);
	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */
	if( v4 && FvValidateIp4(z,n) ){ return 1; }
	if( v6 && FvValidateIp6(z,n) ){ return 1; }
	return 0;
}
/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */
static int FvValidateMac(const char *z,int n){
	char sep;
	int i;
	if( n!=17 ){ return 0; }
	sep = z[2];
	if( sep!=':' && sep!='-' ){ return 0; }
	for( i=0; i<17; i++ ){
		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }
		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }
	}
	return 1;
}
/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local
 * parts or IP-literal domains). */
static int FvValidateEmail(const char *z,int n){
	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;
	const char *zDom;
	if( n==0 || n>320 ){ return 0; }
	for( i=0; i<n; i++ ){
		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }
	}
	if( at<=0 || at==n-1 ){ return 0; } /* one '@', non-empty local and domain */
	localLen = at;
	zDom = z + at + 1;
	domLen = n - at - 1;
	if( z[0]=='.' || z[at-1]=='.' ){ return 0; }
	for( i=0; i<localLen; i++ ){
		unsigned char c = (unsigned char)z[i];
		if( c<=' ' ){ return 0; }
		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }
	}
	if( zDom[0]=='.' || zDom[domLen-1]=='.' ){ return 0; }
	labelStart = 0;
	for( i=0; i<=domLen; i++ ){
		if( i==domLen || zDom[i]=='.' ){
			int ll = i - labelStart;
			if( ll==0 ){ return 0; } /* consecutive dots */
			if( zDom[labelStart]=='-' || zDom[i-1]=='-' ){ return 0; }
			if( i<domLen ){ dotCount++; }
			labelStart = i+1;
		}else{
			unsigned char c = (unsigned char)zDom[i];
			if( !((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-') ){ return 0; }
		}
	}
	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */
	return 1;
}
/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */
static int FvValidateDomain(const char *z,int n){
	int i;
	if( n<1 || n>253 || z[0]=='.' ){ return 0; }
	for( i=0; i<n; i++ ){
		unsigned char c = (unsigned char)z[i];
		if( c<=' ' ){ return 0; }
		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }
	}
	return 1;
}
/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself
 * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */
static int FvValidateUrl(const char *z,int n){
	SyhttpUri sUri;
	if( n==0 ){ return 0; }
	SyZero(&sUri,(sxu32)sizeof(sUri));
	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }
	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;
}
/* The Fv sanitizers build their result by appending directly to the call
 * context (ph7_result_string accumulates, like htmlspecialchars), emitting each
 * kept run in one call and seeding "" so an all-stripped input yields "". */
/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */
static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){
	int i, runStart = 0;
	ph7_result_string(pCtx,"",0);
	for( i=0; i<n; i++ ){
		char c = z[i];
		int keep = (c>='0'&&c<='9') || c=='+' || c=='-';
		if( !keep && isFloat ){
			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))
			    || (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))
			    || ((c=='e'||c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));
		}
		if( !keep ){
			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }
			runStart = i+1;
		}
	}
	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }
}
/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared
 * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops
 * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.
 * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */
static int FvStripByte(unsigned char c,int flags){
	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }
	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }
	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }
	return 0;
}
/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the
 * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified
 * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then
 * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)
 * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW|ENCODE_LOW
 * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH
 * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */
static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){
	int i, runStart = 0;
	ph7_result_string(pCtx,"",0);
	for( i=0; i<n; i++ ){
		unsigned char c = (unsigned char)z[i];
		if( FvStripByte(c,flags) ){
			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }
			runStart = i+1;
			continue;
		}
		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){
			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }
			ph7_result_string(pCtx,"&#38;",-1);
			runStart = i+1;
		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))
		       || (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){
			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }
			ph7_result_string_format(pCtx,"&#%d;",(int)c);
			runStart = i+1;
		}
	}
	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }
}
/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a
 * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes
 * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128
 * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the
 * FULL variant is). Byte-exact vs php 8.5.7. */
static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){
	int i, runStart = 0;
	const char *zEnt;
	ph7_result_string(pCtx,"",0);
	for( i=0; i<n; i++ ){
		unsigned char c = (unsigned char)z[i];
		if( FvStripByte(c,flags) ){
			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }
			runStart = i+1;
			continue;
		}
		switch( c ){
		case '<':  zEnt = "&#60;"; break;
		case '>':  zEnt = "&#62;"; break;
		case '&':  zEnt = "&#38;"; break;
		case '"':  zEnt = "&#34;"; break;
		case '\'': zEnt = "&#39;"; break;
		default:
			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when
			 * ENCODE_HIGH is set. Everything else stays in the current run. */
			if( c<32 || (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){
				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }
				ph7_result_string_format(pCtx,"&#%d;",(int)c);
				runStart = i+1;
			}
			continue; /* keep in the current run */
		}
		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }
		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */
		runStart = i+1;
	}
	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }
}
/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware
 * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.
 * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the
 * default document type); the five inline specials <>&"' are handled separately,
 * so every entry here is a codepoint >=0xA0. 248 rows. */
static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {
	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},
	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},
	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},
	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},
	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},
	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},
	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},
	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},
	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},
	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},
	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},
	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},
	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},
	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},
	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},
	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},
	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},
	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},
	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},
	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},
	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},
	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},
	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},
	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},
	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},
	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},
	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},
	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},
	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},
	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},
	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},
	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},
	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},
	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},
	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},
	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},
	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},
	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},
	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},
	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},
	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},
	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},
	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},
	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},
	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},
	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},
	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},
	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},
	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},
	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},
	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},
	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},
	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},
	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},
	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},
	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},
	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},
	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},
	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},
	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},
	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},
	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}
};
/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */
static const char *FvHtml401Lookup(sxu32 cp){
	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;
	while( lo <= hi ){
		int mid = (lo + hi) / 2;
		sxu32 c = aHtml401Ent[mid].cp;
		if( c == cp ){ return aHtml401Ent[mid].zEnt; }
		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }
	}
	return 0;
}
/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte
 * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,
 * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches
 * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */
static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){
	unsigned char c = p[0];
	if( c < 0x80 ){ *pCp = c; return 1; }
	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */
	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */
		if( zEnd-p < 2 || (p[1]&0xC0)!=0x80 ){ return 0; }
		*pCp = ((sxu32)(c&0x1F)<<6) | (p[1]&0x3F);
		return 2;
	}
	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */
		sxu32 cp;
		if( zEnd-p < 3 || (p[1]&0xC0)!=0x80 || (p[2]&0xC0)!=0x80 ){ return 0; }
		cp = ((sxu32)(c&0x0F)<<12) | ((sxu32)(p[1]&0x3F)<<6) | (p[2]&0x3F);
		if( cp < 0x800 || (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }
		*pCp = cp;
		return 3;
	}
	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */
		sxu32 cp;
		if( zEnd-p < 4 || (p[1]&0xC0)!=0x80 || (p[2]&0xC0)!=0x80 || (p[3]&0xC0)!=0x80 ){ return 0; }
		cp = ((sxu32)(c&0x07)<<18) | ((sxu32)(p[1]&0x3F)<<12) | ((sxu32)(p[2]&0x3F)<<6) | (p[3]&0x3F);
		if( cp < 0x10000 || cp > 0x10FFFF ){ return 0; }
		*pCp = cp;
		return 4;
	}
	return 0;                                /* 0xF5-0xFF */
}
/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes
 * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),
 * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;
 * valid codepoints without a named entity (and low control bytes) pass through
 * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".
 * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).
 * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,
 * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —
 * exactly htmlentities(ENT_QUOTES|ENT_HTML401, double_encode: false), so this
 * delegates to the shared encoder. Byte-exact vs php 8.5.7. */
static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){
	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;
	/* filter_var's FULL_SPECIAL_CHARS has no charset argument: php runs it in the
	 * default charset. */
	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/,PH7_HTML_CS_UTF8);
}
/* ---------------------------------------------------------------------------
 * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).
 * Prototyped next to the five builtins earlier in this file; lives here so it
 * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var
 * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).
 * ------------------------------------------------------------------------ */
/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.
 * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */
static int HtmlCpUtf8(sxu32 cp,char *zBuf){
	sxu8 *z = (sxu8 *)zBuf;
	SX_WRITE_UTF8(z,cp);
	return (int)(z - (sxu8 *)zBuf);
}
/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a
 * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401
 * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the
 * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR
 * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every
 * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */
static int HtmlCpAllowed(sxu32 cp,int iFlags){
	int iDoc = iFlags & PH7_ENT_DOC_MASK;
	if( cp==0x09 || cp==0x0A ){ return 1; }
	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }
	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }
	if( cp < 0x20 || cp > 0x10FFFF ){ return 0; }
	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }
	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 || iDoc == PH7_ENT_DOC_XHTML; }
	if( iDoc == PH7_ENT_DOC_XML1 || iDoc == PH7_ENT_DOC_XHTML ){
		return cp!=0xFFFE && cp!=0xFFFF;
	}
	if( iDoc == PH7_ENT_DOC_HTML5 ){
		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }
		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }
	}
	return 1;
}
/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the
 * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed
 * keeps a literal "\r" verbatim under ENT_HTML5|ENT_DISALLOWED while the
 * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */
static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){
	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }
	return HtmlCpAllowed(cp,iFlags);
}
/* Numeric-reference validity for the double_encode=false "is this already a
 * valid entity" test — a MUCH looser predicate than the decode gate above:
 * any codepoint <= U+10FFFF is valid (controls and surrogates included, every
 * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode
 * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and
 * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)
 * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144
 * (XML1+DISALLOWED) re-encodes &#xD800;. */
static int HtmlNumericAllowed(sxu32 cp,int iFlags){
	if( cp > 0x10FFFF ){ return 0; }
	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }
	if( (iFlags & PH7_ENT_DISALLOWED)
	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)
	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }
	return 1;
}
/* How many bytes the malformed UTF-8 sequence at p consumes — php's
 * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop
 * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats
 * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could
 * start a new sequence is left for the next round. */
static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }
static int HtmlUtf8Lead(unsigned char c){ return c<0x80 || (c>=0xC2 && c<=0xF4); }
static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){
	unsigned char c = p[0];
	int nAvail = (int)(zEnd - p);
	if( c < 0xC2 || c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */
	if( c < 0xE0 ){
		if( nAvail < 2 ){ return 1; }
		return HtmlUtf8Lead(p[1]) ? 1 : 2;
	}
	if( c < 0xF0 ){
		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){
			return 3; /* complete but overlong/surrogate */
		}
		if( nAvail < 2 || HtmlUtf8Lead(p[1]) ){ return 1; }
		if( nAvail < 3 || HtmlUtf8Lead(p[2]) ){ return 2; }
		return 3;
	}
	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){
		return 4; /* complete but overlong / > U+10FFFF */
	}
	if( nAvail < 2 || HtmlUtf8Lead(p[1]) ){ return 1; }
	if( nAvail < 3 || HtmlUtf8Lead(p[2]) ){ return 2; }
	if( nAvail < 4 || HtmlUtf8Lead(p[3]) ){ return 3; }
	return 4;
}
/* The basic special entities, shared by named matching, the hsc_decode
 * numeric whitelist and the translation-table builder so the sets can never
 * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */
static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {
	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}
};
/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has
 * no named entities beyond the specials; XHTML/HTML5 are approximated by the
 * HTML 4.01 table (documented divergence). */
static int HtmlDocHasNamedTable(int iDoc){
	return iDoc != PH7_ENT_DOC_XML1;
}
/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every
 * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities
 * (bEntities) keeps &#039; under XHTML too. The translation table mirrors
 * whichever function the requested table belongs to. */
static const char *HtmlAposEntity(int iDoc,int bEntities){
	if( iDoc == PH7_ENT_DOC_HTML401 || (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){
		return "&#039;";
	}
	return "&apos;";
}
/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the
 * html_entity_decode set (doctype named table + any allowed numeric ref) vs
 * the htmlspecialchars_decode set (the basic specials + quote numerics only).
 * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);
 * numeric refs accept dec/hex (x or X) with any number of leading zeros but
 * reject out-of-range, surrogate and doctype-disallowed codepoints (the
 * caller then leaves the source verbatim). Quote-flag gating is NOT applied
 * here — the same routine doubles as the "is this a valid entity" test for
 * double_encode=false, which ignores the quote bits (oracle-pinned).
 * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that
 * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.
 * On success sets *pCp / *pnConsumed and returns 1. */
static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,
                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){
	int nAvail = (int)(zEnd - z);
	int iDoc = iFlags & PH7_ENT_DOC_MASK;
	sxu32 n;
	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */
	if( z[1] == '#' ){
		/* Numeric reference */
		sxu32 cp = 0;
		int i = 2, bHex = 0, nDig = 0;
		if( z[i]=='x' || z[i]=='X' ){ bHex = 1; i++; }
		for( ; i < nAvail && z[i] != ';' ; i++ ){
			int v;
			unsigned char c = z[i];
			if( c>='0' && c<='9' ){ v = c - '0'; }
			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }
			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }
			else { return 0; }
			/* Stop accumulating once out of range (keeps validating the shape;
			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */
			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }
			nDig++;
		}
		if( nDig == 0 || i >= nAvail ){ return 0; } /* no digits / no ';' */
		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }
		if( !bFull ){
			/* hsc_decode: numeric refs to the five specials only. */
			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}
			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }
		}
		*pCp = cp;
		*pnConsumed = i + 1;
		return 1;
	}
	/* Named reference — every entity name starts with a letter, so anything
	 * else can bail out before touching the tables. */
	if( !((z[1]>='a' && z[1]<='z') || (z[1]>='A' && z[1]<='Z')) ){ return 0; }
	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){
		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }
		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){
			*pCp = aHtmlSpecEnt[n].cp;
			*pnConsumed = aHtmlSpecEnt[n].n;
			return 1;
		}
	}
	if( bFull && HtmlDocHasNamedTable(iDoc) ){
		/* Linear scan of the 248-row table: runs only at '&'-then-letter
		 * positions and guarantees the decode set can never drift from the
		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp
		 * for ~96% of rows. */
		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){
			sxu32 nEnt;
			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }
			nEnt = SyStrlen(aHtml401Ent[n].zEnt);
			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){
				*pCp = aHtml401Ent[n].cp;
				*pnConsumed = (int)nEnt;
				return 1;
			}
		}
	}
	return 0;
}
/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).
 * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),
 * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole
 * result is "" (pre-validated in a first pass: the accumulating result API
 * cannot roll back — same reason FvSanitizeFull is two-pass). */
PH7_PRIVATE void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,
                       int iFlags,int bAll,int bDoubleEncode,int iCs){
	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);
	const unsigned char *p = (const unsigned char *)zIn;
	const unsigned char *runStart;
	int iDoc = iFlags & PH7_ENT_DOC_MASK;
	/* ENT_DISALLOWED replaces a character the doctype forbids with U+FFFD — as a
	 * CHARACTER where the charset can hold one, and as the numeric REFERENCE for
	 * it where it cannot, which is every single-byte charset. */
	const char *zRepl = (iCs == PH7_HTML_CS_LATIN1) ? "&#xFFFD;" : "\xEF\xBF\xBD";
	sxu32 cp;
	if( iCs == PH7_HTML_CS_UTF8 && (iFlags & (PH7_ENT_IGNORE|PH7_ENT_SUBSTITUTE)) == 0 ){
		/* Pass 1: any malformed sequence rejects the entire input. ASCII
		 * bytes cannot be malformed, so skip them without the decoder.
		 * A single-byte charset has no malformed sequences to find. */
		while( p < zEnd ){
			int len;
			if( *p < 0x80 ){ p++; continue; }
			len = FvUtf8Next(p,zEnd,&cp);
			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }
			p += len;
		}
		p = (const unsigned char *)zIn;
	}
	runStart = p;
	ph7_result_string(pCtx,"",0);
	while( p < zEnd ){
		const char *zEnt = 0;
		int len;
		if( *p < 0x80 ){
			len = 1;
			switch( *p ){
			case '<': zEnt = "&lt;"; break;
			case '>': zEnt = "&gt;"; break;
			case '&':
				zEnt = "&amp;";
				if( !bDoubleEncode ){
					sxu32 eCp; int nEat;
					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){
						/* A valid existing entity: keep it verbatim. */
						zEnt = 0;
						len = nEat;
					}
				}
				break;
			case '"':
				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }
				break;
			case '\'':
				if( iFlags & PH7_ENT_QUOTE_SINGLE ){
					zEnt = HtmlAposEntity(iDoc,bAll);
				}
				break;
			default:
				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){
					zEnt = zRepl;
				}
				break;
			}
		}else if( iCs == PH7_HTML_CS_LATIN1 ){
			/* Every byte is a character and its value IS the code point. */
			len = 1;
			cp = *p;
			if( bAll && HtmlDocHasNamedTable(iDoc) ){
				zEnt = FvHtml401Lookup(cp);
			}
			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){
				zEnt = zRepl;
			}
		}else{
			len = FvUtf8Next(p,zEnd,&cp);
			if( len == 0 ){
				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1
				 * would have rejected): drop it or emit ONE U+FFFD for the
				 * whole unit (php substitutes per maximal invalid subpart). */
				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }
				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }
				p += HtmlUtf8FailAdvance(p,zEnd);
				runStart = p;
				continue;
			}
			if( bAll && HtmlDocHasNamedTable(iDoc) ){
				zEnt = FvHtml401Lookup(cp);
			}
			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){
				zEnt = zRepl;
			}
		}
		if( zEnt ){
			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }
			ph7_result_string(pCtx,zEnt,-1);
			runStart = p + len;
		}
		p += len;
	}
	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }
}
/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode
 * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote
 * bits and left verbatim when suppressed; an invalid entity leaves its '&'
 * verbatim and rescans right after it, which also yields PHP's no-double-
 * decode behavior ("&amp;lt;" -> "&lt;"). */
PH7_PRIVATE void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,
                         int iFlags,int bFull,int iCs){
	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);
	const unsigned char *p = (const unsigned char *)zIn;
	const unsigned char *runStart = p;
	ph7_result_string(pCtx,"",0);
	while( p < zEnd ){
		sxu32 cp;
		int nEat;
		if( *p != '&' ){ p++; continue; }
		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }
		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)
		 || (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){
			/* Suppressed quote: leave the entity source verbatim. */
			p += nEat;
			continue;
		}
		if( iCs == PH7_HTML_CS_LATIN1 && cp > 0xFF ){
			/* The charset cannot hold it, so php leaves the entity SOURCE alone —
			 * `&hearts;` stays `&hearts;` in a Latin-1 document. */
			p += nEat;
			continue;
		}
		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }
		if( iCs == PH7_HTML_CS_LATIN1 ){
			char zByte = (char)cp;
			ph7_result_string(pCtx,&zByte,1);
		}else{
			char zBuf[4];
			int n = HtmlCpUtf8(cp,zBuf);
			ph7_result_string(pCtx,zBuf,n);
		}
		p += nEat;
		runStart = p;
	}
	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }
}
/* Resolve the optional charset argument at apArg[idx] to a PH7_HTML_CS_* code.
 *
 * The argument was SCREENED and then dropped: every charset but UTF-8 warned and
 * was answered in UTF-8, so `htmlentities($s, ENT_QUOTES, 'ISO-8859-1')` — the
 * ordinary call for a Latin-1 page — answered a table of 253 rows where php
 * answers 101, encoded a Latin-1 byte as a broken UTF-8 sequence, and decoded
 * `&eacute;` to two bytes where php writes one.
 *
 * ISO-8859-1 is a real charset here now (one byte per character, and its VALUE is
 * the code point). php also supports several other single-byte charsets and the
 * Asian multibyte ones; those stay behind the same UTF-8/Latin-1/ASCII scope cut
 * the mb_ and iconv work draws, and keep php's own unsupported-charset warning. */
PH7_PRIVATE int HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){
	const char *zCs;
	int nCs;
	if( nArg <= idx || ph7_value_is_null(apArg[idx]) ){ return PH7_HTML_CS_UTF8; }
	zCs = ph7_value_to_string(apArg[idx],&nCs);
	if( nCs == 0 ){ return PH7_HTML_CS_UTF8; } /* "" selects the default charset */
	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){
		return PH7_HTML_CS_UTF8; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */
	}
	/* php's own alias set for Latin-1, matched the way php matches it. */
	if( (nCs == 10 && SyStrnicmp(zCs,"ISO-8859-1",10) == 0)
	 || (nCs == 9  && SyStrnicmp(zCs,"ISO8859-1",9) == 0) ){
		return PH7_HTML_CS_LATIN1;
	}
	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);
	return PH7_HTML_CS_UTF8;
}
/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.
 * The five specials come first in byte order, then — for HTML_ENTITIES with a
 * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned
 * ordering; 253 entries under the defaults). */
static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){
	ph7_value_string(pValue,zEnt,-1);
	ph7_array_add_strkey_elem(pArray,zKey,pValue);
	ph7_value_reset_string_cursor(pValue);
}
PH7_PRIVATE void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags,int iCs){
	ph7_value *pArray,*pValue;
	int iDoc = iFlags & PH7_ENT_DOC_MASK;
	sxu32 n;
	pValue = ph7_context_new_scalar(pCtx);
	pArray = ph7_context_new_array(pCtx);
	if( pValue == 0 || pArray == 0 ){
		ph7_result_null(pCtx);
		return;
	}
	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){
		HtmlTableAdd(pArray,pValue,"\"","&quot;");
	}
	HtmlTableAdd(pArray,pValue,"&","&amp;");
	if( iFlags & PH7_ENT_QUOTE_SINGLE ){
		/* The apostrophe row mirrors the function each table belongs to:
		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows
		 * htmlentities (oracle-pinned at flags 35). */
		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));
	}
	HtmlTableAdd(pArray,pValue,"<","&lt;");
	HtmlTableAdd(pArray,pValue,">","&gt;");
	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){
		char zKey[8];
		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){
			int nK;
			if( iCs == PH7_HTML_CS_LATIN1 ){
				/* One byte per character, and only the characters the charset HAS:
				 * php's Latin-1 table is the 96 rows below U+0100 plus the
				 * specials, 101 in all. */
				if( aHtml401Ent[n].cp > 0xFF ){
					continue;
				}
				zKey[0] = (char)aHtml401Ent[n].cp;
				nK = 1;
			}else{
				nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);
			}
			zKey[nK] = 0;
			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);
		}
	}
	ph7_result_value(pCtx,pArray);
}
static int FvEmailAllowed(unsigned char c){
	if( (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9') ){ return 1; }
	return c=='!'||c=='#'||c=='$'||c=='%'||c=='&'||c=='\''||c=='*'||c=='+'
	    || c=='-'||c=='='||c=='?'||c=='^'||c=='_'||c=='`'||c=='{'||c=='|'
	    || c=='}'||c=='~'||c=='@'||c=='.'||c=='['||c==']';
}
static int FvUrlAllowed(unsigned char c){
	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */
}
/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */
static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){
	int i, runStart = 0;
	ph7_result_string(pCtx,"",0);
	for( i=0; i<n; i++ ){
		unsigned char c = (unsigned char)z[i];
		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){
			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }
			runStart = i+1;
		}
	}
	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }
}
/*
 * Apply the selected filter to one already-resolved input value and write the
 * result into pCtx. Shared by filter_var() and filter_input(): the caller has
 * already parsed $filter/$flags/$options. On validation failure the 'default'
 * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,
 * else false. A validating filter that passes returns the (string) input
 * unchanged; a sanitizer writes its transformed output directly.
 */
static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,
                         int iFilter,int iFlags,ph7_value *pOpts,
                         ph7_value *pDefault,const char *zFunc)
{
	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;
	const char *zVal; int nVal;
	/* An array/object input fails every scalar filter. */
	if( ph7_value_is_array(pInput) ){ goto fail; }
	zVal = ph7_value_to_string(pInput,&nVal);
	switch( iFilter ){
	case FV_VALIDATE_INT: {
		ph7_int64 v;
		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }
		if( pOpts ){
			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);
			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);
			/* Read through a COPY: ph7_value_to_int64() would rewrite the entry in
			 * the caller's own $options array (php's zval_get_long() does not). */
			if( pMin && v<PH7_ValuePeekInt64(pMin) ){ goto fail; }
			if( pMax && v>PH7_ValuePeekInt64(pMax) ){ goto fail; }
		}
		ph7_result_int64(pCtx,v);
		return PH7_OK;
	}
	case FV_VALIDATE_FLOAT: {
		double d;
		int decSep = '.';
		const char *zSep = "',."; int nSep = 3;
		/* php reads "decimal"/"thousand" only when the option IS a string — an int,
		 * a bool, null or an array leaves the defaults in place rather than being
		 * cast — and rejects a decimal that is not exactly one byte, or an empty
		 * separator set, with a ValueError naming the calling function. */
		if( pOpts ){
			ph7_value *pDec = ph7_array_fetch(pOpts,"decimal",(int)sizeof("decimal")-1);
			ph7_value *pSep = ph7_array_fetch(pOpts,"thousand",(int)sizeof("thousand")-1);
			if( pDec && ph7_value_is_string(pDec) ){
				int nDec; const char *zDec = ph7_value_to_string(pDec,&nDec); /* already a string: no conversion */
				if( nDec!=1 ){
					return PH7_VmThrowException(pCtx,"ValueError",
						"%s(): \"decimal\" option must be one character long",zFunc);
				}
				decSep = (unsigned char)zDec[0];
			}
			if( pSep && ph7_value_is_string(pSep) ){
				zSep = ph7_value_to_string(pSep,&nSep);
				if( nSep<1 ){
					return PH7_VmThrowException(pCtx,"ValueError",
						"%s(): \"thousand\" option must not be empty",zFunc);
				}
			}
		}
		if( !FvValidateFloat(zVal,nVal,iFlags,decSep,zSep,nSep,&d) ){ goto fail; }
		/* php's range check is the same one the int filter carries, read as a
		 * double (a non-numeric option casts to 0.0, php's own zval_get_double). */
		if( pOpts ){
			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);
			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);
			if( pMin && d<PH7_ValuePeekReal(pMin) ){ goto fail; }
			if( pMax && d>PH7_ValuePeekReal(pMax) ){ goto fail; }
		}
		ph7_result_double(pCtx,d);
		return PH7_OK;
	}
	case FV_VALIDATE_BOOLEAN: {
		int b;
		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }
		ph7_result_bool(pCtx,b);
		return PH7_OK;
	}
	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;
	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;
	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;
	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;
	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;
	case FV_VALIDATE_REGEXP: {
#ifdef PH7_ENABLE_PCRE
		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;
		const char *zRe; int nRe, matched = 0;
		if( pRe==0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): \"regexp\" option is missing",zFunc);
		}
		zRe = ph7_value_to_string(pRe,&nRe);
		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK || !matched ){ goto fail; }
		goto pass;
#else
		goto fail;
#endif
	}
	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;
	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;
	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;
	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;
	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;
	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;
	case FV_DEFAULT:
		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a
		 * STRIP/ENCODE flag is set, in which case apply the string filter. */
		if( iFlags & FV_FLAG_STRING_MASK ){
			FvSanitizeString(pCtx,zVal,nVal,iFlags);
			return PH7_OK;
		}
		goto pass;
	default:
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Unknown filter with ID %d",iFilter);
		break; /* unknown filter id -> fail */
	}
fail:
	if( pDefault ){ ph7_result_value(pCtx,pDefault); }
	else if( bNull ){ ph7_result_null(pCtx); }
	else { ph7_result_bool(pCtx,0); }
	return PH7_OK;
pass: /* validation passed: return the (string) input unchanged */
	ph7_result_string(pCtx,zVal,nVal);
	return PH7_OK;
}
/*
 * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out
 * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a
 * plain flags int, or an array with 'flags' and an 'options' sub-array (whose
 * 'default' entry is the fallback value). Fills the four output pointers;
 * unset outputs keep the caller-provided defaults.
 */
static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,
                              int *piFilter,int *piFlags,
                              ph7_value **ppOpts,ph7_value **ppDefault)
{
	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }
	if( nArg>iBase+1 ){
		if( ph7_value_is_array(apArg[iBase+1]) ){
			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);
			if( pF ){ *piFlags = (int)PH7_ValuePeekInt64(pF); }
			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);
			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }
			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }
		}else{
			*piFlags = ph7_value_to_int(apArg[iBase+1]);
		}
	}
}
/*
 * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)
 *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.
 */
PH7_PRIVATE int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iFilter = FV_DEFAULT, iFlags = 0;
	ph7_value *pOpts = 0, *pDefault = 0;
	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }
	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);
	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault,"filter_var");
}
/*
 * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)
 *  Look up $var_name in the requested INPUT_* superglobal, then apply the
 *  filter. Semantics verified byte-for-byte against php 8.5:
 *   - variable NOT set: 'default' option wins, else false when
 *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are
 *     INVERTED relative to a present value that fails validation, which yields
 *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)
 *   - variable present: delegate to FvApplyFilter.
 *  Divergence: php reads a SAPI snapshot of the original request variables
 *  captured at startup; PHL reads the live superglobal. In CLI they match for
 *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added
 *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in
 *  php's snapshot.
 */
PH7_PRIVATE int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iType, iFilter = FV_DEFAULT, iFlags = 0;
	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;
	const char *zVar, *zSuper; int nVar; sxu32 nSuper;
	if( nArg<2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"filter_input() expects at least 2 arguments, %d given",nArg);
	}
	iType = ph7_value_to_int(apArg[0]);
	switch( iType ){
	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */
	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */
	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */
	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */
	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */
	default:
		return PH7_VmThrowException(pCtx,"ValueError",
			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");
	}
	zVar = ph7_value_to_string(apArg[1],&nVar);
	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);
	/* Resolve the variable from the superglobal (missing/non-array -> not set). */
	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);
	pElem = (pSuper && ph7_value_is_array(pSuper))
		? ph7_array_fetch(pSuper,zVar,nVar) : 0;
	if( pElem==0 ){
		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the
		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */
		if( pDefault ){ ph7_result_value(pCtx,pDefault); }
		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }
		else { ph7_result_null(pCtx); }
		return PH7_OK;
	}
	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault,"filter_input");
}
#endif /* PH7_NEED_BUILTIN_REG */
#ifdef PH7_NEED_FMT_AND_INI
/*
 * The incremental half of the parser below: walk a partially-read record and
 * answer whether it ends INSIDE an enclosure, which is how fgetcsv() knows the
 * value contains the newline and the record continues on the next line.
 *
 * It carries its position and state across appended chunks on purpose. Deciding
 * it by re-parsing the whole accumulated record after every line is quadratic,
 * and one stray quote in a large file is exactly the input that triggers it.
 * The trailing line ending PH7_ProcessCsv strips cannot close an enclosure, so
 * this scan ignores it and both agree on every record.
 */
PH7_PRIVATE void PH7_CsvScanInit(PH7_CsvScan *pScan)
{
	pScan->iState = 0;
	pScan->nPos = 0;
}
PH7_PRIVATE int PH7_CsvScanOpen(PH7_CsvScan *pScan,const char *zIn,sxu32 nByte,
	int delim,int encl,int escape)
{
	sxu32 i = pScan->nPos;
	while( i < nByte ){
		int c = (unsigned char)zIn[i];
		switch( pScan->iState ){
		case 0: /* at a field start: whitespace only counts as padding when an
		         * enclosure is what it leads to */
			if( c != delim && SyisSpace(c) ){
				i++;
				continue;
			}
			if( c == encl ){
				pScan->iState = 2;
				i++;
				continue;
			}
			if( c == delim ){
				i++;
				continue;
			}
			pScan->iState = 1;
			continue;
		case 1: /* unquoted field: an enclosure here is ordinary content */
			if( c == delim ){
				pScan->iState = 0;
			}
			i++;
			continue;
		case 2: /* inside the enclosure */
			if( c == encl ){
				if( i + 1 < nByte && (unsigned char)zIn[i+1] == encl ){
					i += 2;   /* doubled: a literal enclosure */
					continue;
				}
				pScan->iState = 3;
				i++;
				continue;
			}
			if( escape != PH7_CSV_NO_ESCAPE && c == escape ){
				i += (i + 1 < nByte) ? 2 : 1;
				continue;
			}
			i++;
			continue;
		default: /* past the closing enclosure, up to the delimiter */
			if( c == delim ){
				pScan->iState = 0;
			}
			i++;
			continue;
		}
	}
	pScan->nPos = i;
	return pScan->iState == 2;
}
/*
 * Strip ONE trailing line ending -- "\r\n", "\n" or "\r" -- and answer the
 * length left. php applies it to the whole line before parsing, and again to
 * each UNQUOTED field's own content (which is how a lone "\n" reads back as the
 * empty string while "a\r\rb" keeps both of its carriage returns).
 */
static int CsvStripEol(const char *zIn,int nByte)
{
	if( nByte > 0 && zIn[nByte-1] == '\n' ){
		nByte--;
		if( nByte > 0 && zIn[nByte-1] == '\r' ){
			nByte--;
		}
	}else if( nByte > 0 && zIn[nByte-1] == '\r' ){
		nByte--;
	}
	return nByte;
}
/*
 * Parse one CSV record and append each field to pArray.
 *
 * A port of php's php_fgetcsv rules, derived from the oracle field by field.
 * PH7's tokenizer answered a different record for most inputs that were not
 * already trivial:
 *  - an EMPTY field was dropped along with its delimiter, so `,a` read as one
 *    column and `a,b,` as two -- every later column shifted;
 *  - a doubled enclosure was not undoubled, and the closing one was located by
 *    a parity toggle, so `"a""b"` came back with its quoting intact;
 *  - the field content was TRIMMED of whitespace and NUL bytes by the consumer,
 *    so `" a "` -- quoted precisely to keep those spaces -- lost them;
 *  - the escape character consumed the byte after it even OUTSIDE an enclosure,
 *    where php gives it no meaning at all.
 * The rules that are not guessable are the ones php's own parser reaches by
 * accident and programs depend on: whitespace before an opening enclosure is
 * skipped (but kept when no enclosure follows), whatever trails a CLOSING
 * enclosure up to the delimiter is APPENDED to the field (`"a"b` is `ab`), an
 * escape keeps BOTH bytes rather than the escaped one alone, and a record whose
 * whole text is empty is a single NULL field rather than an empty string.
 *
 * *pbOpen (optional) reports that the text ran out inside an enclosure, which is
 * how fgetcsv() knows a quoted newline means the record continues on the next
 * line.
 */
PH7_PRIVATE sxi32 PH7_ProcessCsv(
	ph7_value *pArray, /* Fields are appended here */
	const char *zInput, /* Raw input */
	int nByte,  /* Input length */
	int delim,  /* Delimiter */
	int encl,   /* Enclosure */
	int escape, /* Escape character, or PH7_CSV_NO_ESCAPE */
	int *pbOpen /* OUT: the text ended inside an enclosure */
	)
{
	ph7_vm *pVm = pArray->pVm;
	SyBlob sField;
	ph7_value sEntry;
	int nLimit = CsvStripEol(zInput,nByte);
	int i = 0;
	int bFirst = 1;
	if( pbOpen ){
		*pbOpen = 0;
	}
	SyBlobInit(&sField,&pVm->sAllocator);
	for(;;){
		int bQuoted;
		SyBlobReset(&sField);
		/* Whitespace in front of an OPENING enclosure is not part of the field --
		 * but only when an enclosure is what it leads to. */
		if( i < nLimit ){
			int t = i;
			while( t < nLimit && (unsigned char)zInput[t] != delim
			 && SyisSpace((unsigned char)zInput[t]) ){
				t++;
			}
			if( t < nLimit && (unsigned char)zInput[t] == encl ){
				i = t;
			}
		}
		if( bFirst && i >= nLimit ){
			/* A record with no text at all is ONE null field, not an empty one. */
			PH7_MemObjInit(pVm,&sEntry);
			ph7_array_add_elem(pArray,0,&sEntry);
			PH7_MemObjRelease(&sEntry);
			break;
		}
		bFirst = 0;
		bQuoted = (i < nLimit && (unsigned char)zInput[i] == encl);
		if( bQuoted ){
			int bClosed = 0;
			i++;
			while( i < nLimit ){
				int c = (unsigned char)zInput[i];
				/* The ENCLOSURE is tested first, which only shows when the two
				 * are the same character: `str_getcsv('"aa"b', ',', '"', '"')`
				 * closes on the second quote rather than escaping past it. (The
				 * WRITER's order is the other way round -- php's is too.) */
				if( c == encl ){
					if( i + 1 < nLimit && (unsigned char)zInput[i+1] == encl ){
						/* Doubled: one literal enclosure. */
						SyBlobAppend(&sField,(const void *)&zInput[i],sizeof(char));
						i += 2;
						continue;
					}
					i++;
					bClosed = 1;
					break;
				}
				if( escape != PH7_CSV_NO_ESCAPE && c == escape ){
					/* php keeps the escape AND the byte it protects. */
					SyBlobAppend(&sField,(const void *)&zInput[i],sizeof(char));
					i++;
					if( i < nLimit ){
						SyBlobAppend(&sField,(const void *)&zInput[i],sizeof(char));
						i++;
					}
					continue;
				}
				SyBlobAppend(&sField,(const void *)&zInput[i],sizeof(char));
				i++;
			}
			if( !bClosed ){
				/* The text ran out with the enclosure still open, so the line
				 * ending stripped off the top is INSIDE the value: php puts it
				 * back (which is also how a record that continues on the next
				 * line keeps its embedded newline). */
				if( nByte > nLimit ){
					SyBlobAppend(&sField,(const void *)&zInput[nLimit],
						(sxu32)(nByte - nLimit));
				}
				if( pbOpen ){
					*pbOpen = 1;
				}
			}
			/* Whatever trails the closing enclosure belongs to the field too. */
			while( i < nLimit && (unsigned char)zInput[i] != delim ){
				SyBlobAppend(&sField,(const void *)&zInput[i],sizeof(char));
				i++;
			}
		}else{
			int iStart = i;
			int nRaw;
			while( i < nLimit && (unsigned char)zInput[i] != delim ){
				i++;
			}
			nRaw = CsvStripEol(&zInput[iStart],i - iStart);
			if( nRaw > 0 ){
				SyBlobAppend(&sField,(const void *)&zInput[iStart],(sxu32)nRaw);
			}
		}
		PH7_MemObjInitFromString(pVm,&sEntry,0);
		if( SyBlobLength(&sField) > 0 ){
			ph7_value_string(&sEntry,(const char *)SyBlobData(&sField),
				(int)SyBlobLength(&sField));
		}
		ph7_array_add_elem(pArray,0,&sEntry);
		PH7_MemObjRelease(&sEntry);
		if( i < nLimit && (unsigned char)zInput[i] == delim ){
			/* A trailing delimiter still opens one more (empty) field. */
			i++;
			continue;
		}
		break;
	}
	SyBlobRelease(&sField);
	return SXRET_OK;
}
/*
 * Validate a CSV $separator/$enclosure/$escape argument like php 8 and
 * extract its character. $separator/$enclosure must be exactly one
 * character; $escape may also be empty, which disables escape processing
 * (the caller gets PH7_CSV_NO_ESCAPE). The argument is coerced to string
 * first like php's ZPP, so an int 5 separates on "5"; null and array
 * arguments never reach here — the central type screen rejects them.
 * Returns PH7_OK on success; otherwise the ValueError has been thrown and
 * the caller must return the propagated status.
 */
PH7_PRIVATE sxi32 PH7_CsvCharArg(ph7_context *pCtx,ph7_value *pArg,int iArg,
	const char *zName,int bAllowEmpty,int *pChar)
{
	const char *zPtr;
	int n;
	zPtr = ph7_value_to_string(pArg,&n);
	if( n == 1 ){
		/* UNSIGNED: every comparison downstream is against a byte read out of a
		 * field, so a separator of "\xE9" stored as a negative char would match
		 * nothing at all and the field would go out unquoted. */
		*pChar = (unsigned char)zPtr[0];
		return PH7_OK;
	}
	if( n < 1 && bAllowEmpty ){
		*pChar = PH7_CSV_NO_ESCAPE;
		return PH7_OK;
	}
	return PH7_VmThrowException(pCtx,"ValueError",
		"%s(): Argument #%d ($%s) must be %sa single character",
		ph7_function_name(pCtx),iArg,zName,bAllowEmpty ? "empty or " : ""
		);
}
/*
 * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])
 *  Parse a CSV string into an array.
 * Parameters
 *  $input
 *   The string to parse.
 *  $delimiter
 *   Set the field delimiter (one character only).
 *  $enclosure
 *   Set the field enclosure character (one character only).
 *  $escape
 *   Set the escape character (one character only). Defaults as a backslash (\)
 * Return
 *  An indexed array containing the CSV fields or NULL on failure.
 */
PH7_PRIVATE int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zInput;
	ph7_value *pArray;
	int delim  = ',';   /* Delimiter */
	int encl   = '"' ;  /* Enclosure */
	int escape = '\\';  /* Escape character */
	int nLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the raw input */
	zInput = ph7_value_to_string(apArg[0],&nLen);
	if( nArg > 1 ){
		sxi32 rc = PH7_CsvCharArg(pCtx,apArg[1],2,"separator",0,&delim);
		if( rc != PH7_OK ){
			return rc;
		}
		if( nArg > 2 ){
			rc = PH7_CsvCharArg(pCtx,apArg[2],3,"enclosure",0,&encl);
			if( rc != PH7_OK ){
				return rc;
			}
			if( nArg > 3 ){
				rc = PH7_CsvCharArg(pCtx,apArg[3],4,"escape",1,&escape);
				if( rc != PH7_OK ){
					return rc;
				}
			}
		}
	}
	/* Create our array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		/* Surface a fatal instead of silently returning null on OOM */
		return PH7_ContextMemoryError(pCtx);
	}
	/* Parse the raw input */
	PH7_ProcessCsv(pArray,zInput,nLen,delim,encl,escape,0);
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * Extract a tag name from a raw HTML input and insert it in the given
 * container.
 * Refer to [strip_tags()].
 */
static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)
{
	const char *zEnd = &zTag[nByte];
	const char *zPtr;
	SyString sEntry;
	/* Strip tags */
	for(;;){
		while( zTag < zEnd && (zTag[0] == '<' || zTag[0] == '/' || zTag[0] == '?'
			|| zTag[0] == '!' || zTag[0] == '-' || ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){
				zTag++;
		}
		if( zTag >= zEnd ){
			break;
		}
		zPtr = zTag;
		/* Delimit the tag */
		while(zTag < zEnd ){
			if( (unsigned char)zTag[0] >= 0xc0 ){
				/* UTF-8 stream */
				zTag++;
				SX_JMP_UTF8(zTag,zEnd);
			}else if( !SyisAlphaNum(zTag[0]) ){
				break;
			}else{
				zTag++;
			}
		}
		if( zTag > zPtr ){
			/* Perform the insertion */
			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));
			SyStringFullTrim(&sEntry);
			SySetPut(pSet,(const void *)&sEntry);
		}
		/* Jump the trailing '>' */
		zTag++;
	}
	return SXRET_OK;
}
/*
 * Check if the given HTML tag name is present in the given container.
 * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.
 * Refer to [strip_tags()].
 */
static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)
{
	if( SySetUsed(pSet) > 0 ){
		const char *zCur,*zEnd = &zTag[nByte];
		SyString sTag;
		while( zTag < zEnd &&  (zTag[0] == '<' || zTag[0] == '/' || zTag[0] == '?' ||
			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){
			zTag++;
		}
		/* Delimit the tag */
		zCur = zTag;
		while(zTag < zEnd ){
			if( (unsigned char)zTag[0] >= 0xc0 ){
				/* UTF-8 stream */
				zTag++;
				SX_JMP_UTF8(zTag,zEnd);
			}else if( !SyisAlphaNum(zTag[0]) ){
				break;
			}else{
				zTag++;
			}
		}
		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);
		/* Trim leading white spaces and null bytes */
		SyStringLeftTrimSafe(&sTag);
		if( sTag.nByte > 0 ){
			SyString *aEntry,*pEntry;
			sxi32 rc;
			sxu32 n;
			/* Perform the lookup */
			aEntry = (SyString *)SySetBasePtr(pSet);
			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){
				pEntry = &aEntry[n];
				/* Do the comparison */
				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);
				if( !rc ){
					return SXRET_OK;
				}
			}
		}
	}
	/* No such tag */
	return SXERR_NOTFOUND;
}
/*
 * This function tries to return a string [i.e: in the call context result buffer]
 * with all NUL bytes,HTML and PHP tags stripped from a given string.
 * Refer to [strip_tags()].
 */
PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)
{
	const char *zEnd = &zIn[nByte];
	const char *zPtr,*zTag;
	SySet sSet;
	/* initialize the set of allowed tags */
	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));
	if( nTaglen > 0 ){
		/* Set of allowed tags */
		AddTag(&sSet,zTaglist,nTaglen);
	}
	/* Set the empty string */
	ph7_result_string(pCtx,"",0);
	/* Start processing */
	for(;;){
		if(zIn >= zEnd){
			/* No more input to process */
			break;
		}
		zPtr = zIn;
		/* Find a tag */
		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){
			zIn++;
		}
		if( zIn > zPtr ){
			/* Consume raw input */
			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));
		}
		/* Ignore trailing null bytes */
		while( zIn < zEnd && zIn[0] == 0 ){
			zIn++;
		}
		if(zIn >= zEnd){
			/* No more input to process */
			break;
		}
		if( zIn[0] == '<' ){
			sxi32 rc;
			zTag = zIn++;
			/* Delimit the tag */
			while( zIn < zEnd && zIn[0] != '>' ){
				zIn++;
			}
			if( zIn < zEnd ){
				zIn++; /* Ignore the trailing closing tag */
			}
			/* Query the set */
			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));
			if( rc == SXRET_OK ){
				/* Keep the tag */
				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));
			}
		}
	}
	/* Cleanup */
	SySetRelease(&sSet);
	return SXRET_OK;
}
/*
 * string strip_tags(string $str[,string $allowable_tags])
 *   Strip HTML and PHP tags from a string.
 * Parameters
 *  $str
 *  The input string.
 * $allowable_tags
 *  You can use the optional second parameter to specify tags which should not be stripped.
 * Return
 *  Returns the stripped string.
 */
PH7_PRIVATE int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zTaglist = 0;
	const char *zString;
	int nTaglen = 0;
	int nLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Point to the raw string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){
		/* Allowed tag */
		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);
	}
	/* Process input */
	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);
	return PH7_OK;
}
#endif /* PH7_NEED_FMT_AND_INI */
#ifdef PH7_NEED_FMT_AND_INI
/*
 * Parse an INI string.

 * According to wikipedia
 *  The INI file format is an informal standard for configuration files for some platforms or software.
 *  INI files are simple text files with a basic structure composed of "sections" and "properties".
 *  Format
*    Properties
*     The basic element contained in an INI file is the property. Every property has a name and a value
*     delimited by an equals sign (=). The name appears to the left of the equals sign.
*     Example:
*      name=value
*    Sections
*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself
*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.
*     There is no explicit "end of section" delimiter; sections end at the next section declaration
*     or the end of the file. Sections may not be nested.
*     Example:
*      [section]
*   Comments
*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.
* This function return an array holding parsed values on success.FALSE otherwise.
*/
/*
 * The ini scanner's ${NAME} expansion: php answers a known ini OPTION first and
 * the process environment second (zend_ini_get_var), the empty string when
 * neither knows the name — a defined CONSTANT deliberately does NOT answer
 * here (that is the bare-identifier rule below). The VFS environment reader
 * answers through the call context's RESULT slot (it was written for
 * getenv()), so the read borrows pCtx->pRet around the call and empties it
 * again; the parse's own result is not written until the very end.
 */
static void VmIniExpandDollarVar(ph7_context *pCtx,const char *zName,sxu32 nName,SyBlob *pOut)
{
	char zVar[128];
	SyBlob sVal;
	if( nName < 1 || nName >= sizeof(zVar) ){
		return; /* php answers "" for an unknown name; an unreasonable one is unknown */
	}
	SyMemcpy(zName,zVar,nName);
	zVar[nName] = 0;
	SyBlobInit(&sVal,&pCtx->pVm->sAllocator);
	PH7_VmIniGetStr(pCtx->pVm,zVar,&sVal);
	if( SyBlobLength(&sVal) > 0 ){
		SyBlobAppend(pOut,SyBlobData(&sVal),SyBlobLength(&sVal));
		SyBlobRelease(&sVal);
		return;
	}
	SyBlobRelease(&sVal);
	{
		const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;
		ph7_value *pRet = pCtx->pRet;
		sxu32 nBefore = SyBlobLength(&pRet->sBlob);
		if( pVfs && pVfs->xGetenv ){
			if( pVfs->xGetenv(zVar,pCtx) == PH7_OK && SyBlobLength(&pRet->sBlob) > nBefore ){
				SyBlobAppend(pOut,(const char *)SyBlobData(&pRet->sBlob) + nBefore,
					SyBlobLength(&pRet->sBlob) - nBefore);
			}
			ph7_value_reset_string_cursor(pRet);
		}
	}
}
/*
 * Interpret one UNQUOTED ini value the way php's INI_SCANNER_NORMAL and
 * INI_SCANNER_TYPED do (a QUOTED value is always its literal bytes, and RAW
 * never reaches here):
 *
 *  - A whole-value word, case-insensitive: true/on/yes and false/off/no/none
 *    and null. NORMAL renders them "1" / "" / ""; TYPED renders true / false /
 *    NULL.
 *  - TYPED only: -?[0-9]+ is an int — a value int64 cannot hold falls back to
 *    the SOURCE text as a string — and [0-9]*\.[0-9]* with at least one digit
 *    is a float. php's typed grammar attaches '-' only to the INTEGER shape
 *    ("-1.5" stays a string); '+', hex, binary and exponents were never in it.
 *  - Everything else expands: ${NAME} answers an ini option or the
 *    environment, and a bare identifier token that names a DEFINED constant is
 *    replaced by that constant's value ("MYC and more" -> "someval and more").
 *
 * pValue arrives as an empty string.
 */
static void VmIniInterpretValue(ph7_context *pCtx,const SyString *pRaw,int iMode,ph7_value *pValue)
{
	const char *zIn = pRaw->zString;
	const char *zEnd = &zIn[pRaw->nByte];
	sxu32 n = pRaw->nByte;
	SyBlob sOut;
	if( n == 0 ){
		return; /* the empty string, both modes */
	}
	if( (n == 4 && SyStrnicmp(zIn,"true",4) == 0)
	 || (n == 2 && SyStrnicmp(zIn,"on",2) == 0)
	 || (n == 3 && SyStrnicmp(zIn,"yes",3) == 0) ){
		if( iMode == PH7_INI_SCANNER_TYPED ){
			ph7_value_bool(pValue,1);
		}else{
			ph7_value_string(pValue,"1",1);
		}
		return;
	}
	if( (n == 5 && SyStrnicmp(zIn,"false",5) == 0)
	 || (n == 3 && SyStrnicmp(zIn,"off",3) == 0)
	 || (n == 2 && SyStrnicmp(zIn,"no",2) == 0)
	 || (n == 4 && SyStrnicmp(zIn,"none",4) == 0) ){
		if( iMode == PH7_INI_SCANNER_TYPED ){
			ph7_value_bool(pValue,0);
		}
		/* NORMAL: the empty string pValue already holds */
		return;
	}
	if( n == 4 && SyStrnicmp(zIn,"null",4) == 0 ){
		if( iMode == PH7_INI_SCANNER_TYPED ){
			ph7_value_null(pValue);
		}
		return;
	}
	if( iMode == PH7_INI_SCANNER_TYPED ){
		sxu32 i = 0;
		sxu32 nDig = 0,nDot = 0;
		int bNeg = 0,bNum = 1;
		if( zIn[0] == '-' ){
			bNeg = 1;
			i = 1;
		}
		for( ; i < n ; i++ ){
			if( zIn[i] >= '0' && zIn[i] <= '9' ){
				nDig++;
			}else if( zIn[i] == '.' ){
				nDot++;
			}else{
				bNum = 0;
				break;
			}
		}
		if( bNum && nDig > 0 && nDot == 0 ){
			sxi64 iVal = 0;
			int iOverflow = 0;
			SyStrToInt64Ex(zIn,n,(void *)&iVal,0,&iOverflow);
			if( !iOverflow ){
				ph7_value_int64(pValue,iVal);
				return;
			}
			ph7_value_string(pValue,zIn,(int)n);
			return;
		}
		if( bNum && nDig > 0 && nDot == 1 && !bNeg ){
			double rVal = 0;
			SyStrToReal(zIn,n,(void *)&rVal,0);
			ph7_value_double(pValue,rVal);
			return;
		}
	}
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	while( zIn < zEnd ){
		if( zIn[0] == '$' && &zIn[1] < zEnd && zIn[1] == '{' ){
			const char *p = &zIn[2];
			while( p < zEnd && p[0] != '}' ){
				p++;
			}
			if( p < zEnd ){
				VmIniExpandDollarVar(pCtx,&zIn[2],(sxu32)(p - &zIn[2]),&sOut);
				zIn = &p[1];
				continue;
			}
			/* No closing brace: the bytes stand as written */
		}
		if( ((unsigned char)zIn[0] < 0xc0 && SyisAlpha(zIn[0])) || zIn[0] == '_' ){
			const char *pTok = zIn;
			ph7_value sCons;
			while( zIn < zEnd
			 && (((unsigned char)zIn[0] < 0xc0 && SyisAlphaNum(zIn[0])) || zIn[0] == '_') ){
				zIn++;
			}
			PH7_MemObjInit(pCtx->pVm,&sCons);
			if( PH7_VmQueryConstant(pCtx->pVm,pTok,(sxu32)(zIn - pTok),&sCons) ){
				int nCons;
				const char *zCons = ph7_value_to_string(&sCons,&nCons);
				SyBlobAppend(&sOut,zCons,(sxu32)nCons);
			}else{
				SyBlobAppend(&sOut,pTok,(sxu32)(zIn - pTok));
			}
			PH7_MemObjRelease(&sCons);
			continue;
		}
		SyBlobAppend(&sOut,zIn,(sxu32)sizeof(char));
		zIn++;
	}
	if( SyBlobLength(&sOut) > 0 ){
		ph7_value_string(pValue,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	}
	SyBlobRelease(&sOut);
}
PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection,int iScannerMode)
{
	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;
	const char *zCur,*zEnd = &zIn[nByte];
	SyHashEntry *pEntry;
	SyString sEntry;
	SyHash sHash;
	int c;
	/* Create an empty array and worker variables */
	pArray = ph7_context_new_array(pCtx);
	pWorker = ph7_context_new_scalar(pCtx);
	pValue = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pWorker == 0 || pValue == 0){
		/* Out of memory: surface a fatal instead of returning FALSE */
		return PH7_ContextMemoryError(pCtx);
	}
	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);
	pCur = pArray;
	/* Start the parse process */
	for(;;){
		/* Ignore leading white spaces */
		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){
			zIn++;
		}
		if( zIn >= zEnd ){
			/* No more input to process */
			break;
		}
		if( zIn[0] == ';' || zIn[0] == '#' ){
			/* Comment til the end of line */
			zIn++;
			while(zIn < zEnd && zIn[0] != '\n' ){
				zIn++;
			}
			continue;
		}
		/* Reset the string cursor of the working variable */
		ph7_value_reset_string_cursor(pWorker);
		if( zIn[0] == '[' ){
			/* Section: Extract the section name */
			zIn++;
			zCur = zIn;
			while( zIn < zEnd && zIn[0] != ']' ){
				zIn++;
			}
			if( zIn > zCur && bProcessSection ){
				/* Save the section name */
				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));
				SyStringFullTrim(&sEntry);
				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);
				if( sEntry.nByte > 0 ){
					/* Associate an array with the section */
					pSection = ph7_context_new_array(pCtx);
					if( pSection ){
						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);
						pCur = pSection;
					}
				}
			}
			zIn++; /* Trailing square brackets ']' */
		}else{
			ph7_value *pOldCur;
			int is_array;
			int iLen;
			/* Properties */
			is_array = 0;
			zCur = zIn;
			iLen = 0; /* cc warning */
			pOldCur = pCur;
			while( zIn < zEnd && zIn[0] != '=' ){
				if( zIn[0] == '[' && !is_array ){
					/* Array */
					iLen = (int)(zIn-zCur);
					is_array = 1;
					if( iLen > 0 ){
						ph7_value *pvArr = 0; /* cc warning */
						/* Query the hashtable */
						SyStringInitFromBuf(&sEntry,zCur,iLen);
						SyStringFullTrim(&sEntry);
						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);
						if( pEntry ){
							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);
						}else{
							/* Create an empty array */
							pvArr = ph7_context_new_array(pCtx);
							if( pvArr ){
								/* Save the entry */
								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);
								/* Insert the entry */
								ph7_value_reset_string_cursor(pWorker);
								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);
								ph7_array_add_elem(pCur,pWorker,pvArr);
								ph7_value_reset_string_cursor(pWorker);
							}
						}
						if( pvArr ){
							pCur = pvArr;
						}
					}
					while ( zIn < zEnd && zIn[0] != ']' ){
						zIn++;
					}
				}
				zIn++;
			}
			if( !is_array ){
				iLen = (int)(zIn-zCur);
			}
			/* Trim the key */
			SyStringInitFromBuf(&sEntry,zCur,iLen);
			SyStringFullTrim(&sEntry);
			if( sEntry.nByte > 0 ){
				if( !is_array ){
					/* Save the key name */
					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);
				}
				/* extract key value. pValue must come back to an EMPTY STRING
				 * whatever the last entry typed it as (INI_SCANNER_TYPED sets
				 * bool/int/float/null): ph7_value_string() re-types it, the
				 * cursor reset then empties it. */
				ph7_value_string(pValue,"",0);
				ph7_value_reset_string_cursor(pValue);
				zIn++; /* '=' */
				/* Skip the spaces BEFORE the value but never the newline that
				 * ENDS it: `key =` at end of line is php's empty-string entry,
				 * and the old skip ran onto the next line and swallowed it
				 * whole — `e1 =` followed by `c1 = 10K` answered
				 * ["e1" => "c1 = 10K"] with c1 GONE. */
				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && zIn[0] != '\n' && SyisSpace(zIn[0]) ){
					zIn++;
				}
				if( zIn < zEnd && zIn[0] != '\n' ){
					int bQuoted;
					zCur = zIn;
					c = zIn[0];
					bQuoted = (c == '"' || c == '\'');
					if( bQuoted ){
						zIn++;
						/* Delimit the value */
						while( zIn < zEnd ){
							if ( zIn[0] == c && zIn[-1] != '\\' ){
								break;
							}
							zIn++;
						}
						if( zIn < zEnd ){
							zIn++;
						}
					}else{
						while( zIn < zEnd ){
							if( zIn[0] == '\n' ){
								if( zIn[-1] != '\\' ){
									break;
								}
							}else if( zIn[0] == ';' || zIn[0] == '#' ){
								/* Inline comments */
								break;
							}
							zIn++;
						}
					}
					/* Trim the value */
					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));
					SyStringFullTrim(&sEntry);
					if( bQuoted ){
						SyStringTrimLeadingChar(&sEntry,c);
						SyStringTrimTrailingChar(&sEntry,c);
					}
					if( bQuoted || iScannerMode == PH7_INI_SCANNER_RAW ){
						/* A quoted value is its literal bytes in EVERY mode
						 * (php runs no expansion inside quotes), and RAW keeps
						 * even a bare word uninterpreted. */
						if( sEntry.nByte > 0 ){
							ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);
						}
					}else{
						VmIniInterpretValue(pCtx,&sEntry,iScannerMode,pValue);
					}
				}
				/* Insert the key and it's value (an empty value included) */
				ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);
			}else{
				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) || zIn[0] == '=' ) ){
					zIn++;
				}
			}
			pCur = pOldCur;
		}
	}
	SyHashRelease(&sHash);
	/* Return the parse of the INI string */
	ph7_result_value(pCtx,pArray);
	return SXRET_OK;
}
/*
 * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])
 *  Parse a configuration string.
 * Parameters
 *  $ini
 *   The contents of the ini file being parsed.
 *  $process_sections
 *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names
 *   and settings included. The default for process_sections is FALSE.
 *  $scanner_mode
 *   INI_SCANNER_NORMAL (default: values interpreted — booleans, constants,
 *   ${var}), INI_SCANNER_RAW (values kept verbatim) or INI_SCANNER_TYPED
 *   (booleans, null and numbers come back as their own types). Any other
 *   value is php's "Invalid scanner mode" warning and FALSE.
 * Return
 *  The settings are returned as an associative array on success, and FALSE on failure.
 */
PH7_PRIVATE int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIni;
	int nByte;
	int iMode = PH7_INI_SCANNER_NORMAL;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE*/
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){
		iMode = ph7_value_to_int(apArg[2]);
		if( iMode != PH7_INI_SCANNER_NORMAL && iMode != PH7_INI_SCANNER_RAW
		 && iMode != PH7_INI_SCANNER_TYPED ){
			/* php's bare message: no `func(): ` qualifier on this one */
			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"Invalid scanner mode");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
	/* Extract the raw INI buffer */
	zIni = ph7_value_to_string(apArg[0],&nByte);
	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */
	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0,iMode);
}
#endif /* PH7_NEED_FMT_AND_INI */
#ifdef PH7_NEED_BUILTIN_REG

/*
 * Ctype Functions.
 * Status:
 *    Stable.
 */
/*
 * bool ctype_alnum(string $text)
 *  Checks if all of the characters in the provided string, text, are alphanumeric.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.
 */
PH7_PRIVATE int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( !SyisAlphaNum(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_alpha(string $text)
 *  Checks if all of the characters in the provided string, text, are alphabetic.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.
 */
PH7_PRIVATE int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( !SyisAlpha(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_cntrl(string $text)
 *  Checks if all of the characters in the provided string, text, are control characters.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  TRUE if every character in text is a control characters,FALSE otherwise.
 */
PH7_PRIVATE int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisCtrl(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_digit(string $text)
 *  Checks if all of the characters in the provided string, text, are numerical.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.
 */
PH7_PRIVATE int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisDigit(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_xdigit(string $text)
 *  Check for character(s) representing a hexadecimal digit.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text is a hexadecimal 'digit', that is
 * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.
 */
PH7_PRIVATE int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisHex(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_graph(string $text)
 *  Checks if all of the characters in the provided string, text, creates visible output.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text is printable and actually creates visible output
 * (no white space), FALSE otherwise.
 */
PH7_PRIVATE int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisGraph(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_print(string $text)
 *  Checks if all of the characters in the provided string, text, are printable.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text will actually create output (including blanks).
 *  Returns FALSE if text contains control characters or characters that do not have any output
 *  or control function at all.
 */
PH7_PRIVATE int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisPrint(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_punct(string $text)
 *  Checks if all of the characters in the provided string, text, are punctuation character.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text is printable, but neither letter
 *  digit or blank, FALSE otherwise.
 */
PH7_PRIVATE int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisPunct(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_space(string $text)
 *  Checks if all of the characters in the provided string, text, creates whitespace.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.
 *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return
 *  and form feed characters.
 */
PH7_PRIVATE int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisSpace(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_lower(string $text)
 *  Checks if all of the characters in the provided string, text, are lowercase letters.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text is a lowercase letter in the current locale.
 */
PH7_PRIVATE int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( !SyisLower(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_upper(string $text)
 *  Checks if all of the characters in the provided string, text, are uppercase letters.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text is a uppercase letter in the current locale.
 */
PH7_PRIVATE int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( !SyisUpper(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/* Date/Time functions moved to builtin_date.c */
/*
 * Section:
 *    URL handling Functions.
 * Status:
 *    Stable.
 */
/*
 * Output consumer callback for the standard Symisc routines.
 * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].
 */
static int Consumer(const void *pData,unsigned int nLen,void *pUserData)
{
	/* Store in the call context result buffer */
	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);
	return SXRET_OK;
}
/*
 * string base64_encode(string $data)
 *  Encodes data with MIME base64
 * Parameter
 *  $data
 *    Data to encode
 * Return
 *  Encoded data or FALSE on failure.
 */
PH7_PRIVATE int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the input string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* php encodes the empty string to the EMPTY STRING; base64_encode() cannot
		 * fail at all, so FALSE was never one of its answers. */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Perform the BASE64 encoding */
	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);
	return PH7_OK;
}
/*
 * php's base64 reverse table: -1 is skippable whitespace (\t \n \r and space,
 * exactly php's set -- \v/\f are NOT skipped), -2 is an invalid byte, 0..63 the
 * decoded 6-bit value. The pad byte '=' is handled before the lookup, so its
 * table slot is never consulted.
 */
static const signed char aB64Rev[256] = {
	-2,-2,-2,-2,-2,-2,-2,-2,-2,-1,-1,-2,-2,-1,-2,-2,
	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,
	-1,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,62,-2,-2,-2,63,
	52,53,54,55,56,57,58,59,60,61,-2,-2,-2,-2,-2,-2,
	-2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
	15,16,17,18,19,20,21,22,23,24,25,-2,-2,-2,-2,-2,
	-2,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
	41,42,43,44,45,46,47,48,49,50,51,-2,-2,-2,-2,-2,
	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,
	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,
	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,
	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,
	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,
	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,
	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,
	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2
};
/*
 * string base64_decode(string $data, bool $strict = false)
 *  Decodes data encoded with MIME base64
 * Parameters
 *  $data
 *    Encoded data.
 *  $strict
 *    When true, return FALSE if the input contains a character outside the
 *    base64 alphabet (whitespace is still skipped) or the padding/length is
 *    malformed. When false, such bytes are silently skipped (best effort).
 * Return
 *  Returns the original data or FALSE on failure.
 * Implementation note: a faithful port of php's php_base64_decode_ex(). The old
 * code ignored $strict entirely and ran the shared SyBase64Decode(), which maps
 * every non-alphabet byte (whitespace included) to 0 rather than skipping it --
 * a silent wrong answer on padded/whitespace input in BOTH modes.
 */
PH7_PRIVATE int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn;
	unsigned char *zOut;
	int nLen,strict = 0;
	int i = 0,j = 0,padding = 0,k;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the input string */
	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* php decodes the empty string to the EMPTY STRING, not FALSE (FALSE is reserved
		 * for input that cannot be decoded at all). */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		strict = ph7_value_to_bool(apArg[1]);
	}
	/* Output is at most 3/4 of the input; nLen bytes is a safe upper bound. */
	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nLen + 1);
	if( zOut == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	for( k = 0 ; k < nLen ; ++k ){
		int ch = zIn[k];
		int val;
		if( ch == '=' ){
			/* Pad byte: count it, decode nothing. */
			padding++;
			continue;
		}
		val = aB64Rev[ch];
		if( !strict ){
			/* Lenient: skip whitespace AND any invalid byte. */
			if( val < 0 ){
				continue;
			}
		}else{
			if( val == -1 ){
				/* Skippable whitespace. */
				continue;
			}
			if( val == -2 ){
				/* A byte outside the base64 alphabet. */
				goto fail;
			}
			if( padding ){
				/* Data must not follow the padding. */
				goto fail;
			}
		}
		switch( i & 3 ){
			case 0:
				zOut[j] = (unsigned char)(val << 2);
				break;
			case 1:
				zOut[j++] |= (unsigned char)(val >> 4);
				zOut[j] = (unsigned char)((val & 0x0F) << 4);
				break;
			case 2:
				zOut[j++] |= (unsigned char)(val >> 2);
				zOut[j] = (unsigned char)((val & 0x03) << 6);
				break;
			case 3:
				zOut[j++] |= (unsigned char)val;
				break;
		}
		i++;
	}
	if( strict ){
		/* A lone trailing 6-bit group (one leftover char) cannot form a byte. */
		if( (i & 3) == 1 ){
			goto fail;
		}
		/* Padding must be 1 or 2 bytes and complete the 4-char group. */
		if( padding && (padding > 2 || ((i + padding) & 3) != 0) ){
			goto fail;
		}
	}
	ph7_result_string(pCtx,(const char *)zOut,j);
	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);
	return PH7_OK;
fail:
	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * uuencode's six-bit alphabet: a value of 0 is written as the backtick php uses
 * instead of the historical space, every other value as ' ' + value. The three
 * PH7_UU_ENC_C* helpers pack the 6-bit groups exactly like php's macros: each
 * contribution is masked to its own bit window, so the result never depends on
 * whether the platform's char is signed.
 */
#define PH7_UU_ENC(c)      ((char)((c) ? (((c) & 077) + ' ') : '`'))
#define PH7_UU_ENC_C1(a)   PH7_UU_ENC((a) >> 2)
#define PH7_UU_ENC_C2(a,b) PH7_UU_ENC((((a) << 4) & 060) | (((b) >> 4) & 017))
#define PH7_UU_ENC_C3(b,c) PH7_UU_ENC((((b) << 2) & 074) | (((c) >> 6) & 003))
#define PH7_UU_ENC_C4(c)   PH7_UU_ENC((c) & 077)
#define PH7_UU_DEC(c)      ((((int)(c)) - ' ') & 077)
/*
 * string convert_uuencode(string $data)
 *  Uuencode a string.
 * Parameter
 *  $data
 *   Data to encode.
 * Return
 *  The uuencoded data: 45-byte lines, each prefixed with its encoded length and
 *  terminated by a newline, followed by php's "`\n" end marker. An empty input
 *  answers just that marker.
 * Implementation note: a faithful port of php's php_uuencode(). This used to be
 * registered as an ALIAS of base64_encode() -- a wrong ALGORITHM, so every answer
 * was silently a base64 string (convert_uuencode("abc") gave "YWJj" where php
 * gives "#86)C\n`\n").
 */
PH7_PRIVATE int PH7_builtin_convert_uuencode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd,*zStop;
	char zLine[64]; /* one full line is 1 length byte + 60 data bytes + '\n' */
	int nLen,iLen = 45,n;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the input string */
	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 0 ){
		nLen = 0;
	}
	zEnd = &zIn[nLen];
	/* Emit whole groups while at least four bytes remain: the last line is closed by
	 * the tail block below so a group of one or two bytes gets php's '`' filler. */
	while( &zIn[3] < zEnd ){
		zStop = &zIn[iLen];
		if( zStop > zEnd ){
			/* A short final line: its length byte counts every remaining byte, but only
			 * whole three-byte groups are encoded here -- the leftovers ride the tail
			 * block, which then adds no length byte of its own. */
			iLen = (int)(zEnd - zIn);
			zStop = &zIn[(iLen/3)*3];
		}
		n = 0;
		zLine[n++] = PH7_UU_ENC(iLen);
		while( zIn < zStop ){
			zLine[n++] = PH7_UU_ENC_C1(zIn[0]);
			zLine[n++] = PH7_UU_ENC_C2(zIn[0],zIn[1]);
			zLine[n++] = PH7_UU_ENC_C3(zIn[1],zIn[2]);
			zLine[n++] = PH7_UU_ENC_C4(zIn[2]);
			zIn += 3;
		}
		if( iLen == 45 ){
			zLine[n++] = '\n';
		}
		ph7_result_string(pCtx,zLine,n);
	}
	if( zIn < zEnd ){
		/* One to three trailing bytes. php reads the bytes past the end of the string
		 * (its buffers are NUL terminated); the missing ones are zero here. */
		unsigned char c0 = zIn[0];
		unsigned char c1 = (&zIn[1] < zEnd) ? zIn[1] : 0;
		unsigned char c2 = (&zIn[2] < zEnd) ? zIn[2] : 0;
		n = 0;
		if( iLen == 45 ){
			/* No short line was opened above: this group is a line of its own. */
			zLine[n++] = PH7_UU_ENC((int)(zEnd - zIn));
			iLen = 0;
		}
		zLine[n++] = PH7_UU_ENC_C1(c0);
		zLine[n++] = PH7_UU_ENC_C2(c0,c1);
		zLine[n++] = ((zEnd - zIn) > 1) ? PH7_UU_ENC_C3(c1,c2) : '`';
		zLine[n++] = ((zEnd - zIn) > 2) ? PH7_UU_ENC_C4(c2)     : '`';
		ph7_result_string(pCtx,zLine,n);
	}
	if( iLen != 45 ){
		/* A short (or tail) line is still open; a run of whole 45-byte lines -- and the
		 * empty input, which opens no line at all -- is already newline-terminated. */
		ph7_result_string(pCtx,"\n",1);
	}
	/* php's end marker: a zero-length line. */
	ph7_result_string(pCtx,"`\n",2);
	return PH7_OK;
}
/*
 * string|false convert_uudecode(string $data)
 *  Decode a uuencoded string.
 * Parameter
 *  $data
 *   Uuencoded data.
 * Return
 *  The decoded data, or FALSE (with a warning) when $data is not a valid uuencoded
 *  string: an empty input, a line claiming more bytes than the whole input holds, or
 *  a line whose data is truncated. Trailing garbage after the first short line is
 *  ignored, exactly like php.
 * Implementation note: a faithful port of php's php_uudecode(); see the encoder above
 * for why this was not a decoder at all before.
 */
PH7_PRIVATE int PH7_builtin_convert_uudecode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd,*zStop;
	unsigned char *zOut;
	int nLen,iLen;
	sxu32 nOut = 0,nTotal = 0;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the input string */
	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* php refuses the empty string rather than decoding it to "". */
		goto fail;
	}
	zEnd = &zIn[nLen];
	/* Every four input characters yield three bytes and each line spends one more
	 * character on its length, so the input length is a safe upper bound. */
	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nLen + 1);
	if( zOut == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	while( zIn < zEnd ){
		iLen = PH7_UU_DEC(*zIn++);
		if( iLen == 0 ){
			/* The end marker (or any line claiming zero bytes) stops the decoding. */
			break;
		}
		if( iLen > nLen ){
			goto err;
		}
		nTotal += (sxu32)iLen;
		/* A line carries four characters per three-byte group, whole groups only. */
		zStop = zIn + ((iLen + 2)/3)*4;
		if( zStop > zEnd ){
			goto err;
		}
		while( zIn < zStop ){
			zOut[nOut++] = (unsigned char)((PH7_UU_DEC(zIn[0]) << 2) | (PH7_UU_DEC(zIn[1]) >> 4));
			zOut[nOut++] = (unsigned char)((PH7_UU_DEC(zIn[1]) << 4) | (PH7_UU_DEC(zIn[2]) >> 2));
			zOut[nOut++] = (unsigned char)((PH7_UU_DEC(zIn[2]) << 6) |  PH7_UU_DEC(zIn[3]));
			zIn += 4;
		}
		if( iLen < 45 ){
			/* A short line ends the payload; whatever follows is ignored. */
			break;
		}
		zIn++; /* Skip the line separator */
	}
	/* Drop the padding the last group carried: php keeps only as many bytes as the
	 * length bytes declared, counted over the WHOLE input rather than per line. */
	if( nOut > nTotal ){
		nOut = nTotal;
	}
	ph7_result_string(pCtx,(const char *)zOut,(int)nOut);
	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);
	return PH7_OK;
err:
	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);
fail:
	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
		"Argument #1 ($data) is not a valid uuencoded string"); /* the "convert_uudecode(): " prefix is added by the handler */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * string urlencode(string $str)
 *  URL encoding
 * Parameter
 *  $data
 *   Input string.
 * Return
 *  Returns a string in which all non-alphanumeric characters except -_. have
 *  been replaced with a percent (%) sign followed by two hex digits and spaces
 *  encoded as plus (+) signs.
 */
PH7_PRIVATE int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the input string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* php returns an empty string for empty input, not FALSE */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Perform the URL encoding */
	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);
	return PH7_OK;
}
/*
 * string rawurlencode(string $str)
 *  RFC 3986 URL encoding: spaces become %20 (not '+') and '~' is left intact.
 */
PH7_PRIVATE int PH7_builtin_rawurlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the input string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* php returns an empty string for empty input, not FALSE */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Perform the RFC 3986 URL encoding */
	SyUriEncodeRaw(zIn,(sxu32)nLen,Consumer,pCtx);
	return PH7_OK;
}
/* SyUriEncode/SyUriDecode write through a consumer; both query-string builtins
 * below want the bytes in a blob. */
static int UriBlobConsumer(const void *pData,unsigned int nLen,void *pUserData)
{
	return (int)SyBlobAppend((SyBlob *)pUserData,pData,(sxu32)nLen);
}
/* --- parse_str (php's main/php_variables.c) ---------------------------- */

/*
 * php_register_variable_ex(): register ONE decoded "name[idx][idx]" against a
 * target array. The name arrives ALREADY url-decoded, which is the rule the
 * chunk did not have -- php decodes the whole key first and only then looks for
 * brackets, so "a%5Bb%5D=1" is the NESTED a[b], not a flat key spelled "a[b]".
 *
 * The walk is destructive on its own copy of the name (php writes NULs over the
 * brackets), so zVar must be a writable NUL-terminated buffer.
 */
static ph7_hashmap * ParseStrDescend(ph7_context *pCtx,ph7_hashmap *pMap,
	const char *zKey,ph7_value *pKey)
{
	ph7_hashmap_node *pNode = 0;
	ph7_value *pSlot,*pEmpty;
	if( zKey ){
		ph7_value_reset_string_cursor(pKey);
		ph7_value_string(pKey,zKey,(int)SyStrlen(zKey));
		if( PH7_HashmapLookup(pMap,pKey,&pNode) == SXRET_OK ){
			pSlot = HashmapExtractNodeValue(pNode);
			if( pSlot && (pSlot->iFlags & MEMOBJ_HASHMAP) ){
				return (ph7_hashmap *)pSlot->x.pOther;
			}
		}
	}
	/* Nothing usable there: php OVERWRITES whatever scalar is in the way with a
	 * fresh array ("a=1&a[b]=2" ends as a['b']). */
	pEmpty = ph7_context_new_array(pCtx);
	if( pEmpty == 0 || PH7_HashmapInsert(pMap,zKey ? pKey : 0,pEmpty) != SXRET_OK ){
		return 0;
	}
	if( zKey ){
		if( PH7_HashmapLookup(pMap,pKey,&pNode) != SXRET_OK ){
			return 0;
		}
	}else{
		pNode = pMap->pLast;   /* the append just made */
	}
	pSlot = pNode ? HashmapExtractNodeValue(pNode) : 0;
	return (pSlot && (pSlot->iFlags & MEMOBJ_HASHMAP)) ? (ph7_hashmap *)pSlot->x.pOther : 0;
}
static void ParseStrRegister(ph7_context *pCtx,ph7_value *pTarget,char *zVar,
	ph7_value *pVal,int nMaxNest)
{
	ph7_hashmap *pCur = (ph7_hashmap *)pTarget->x.pOther;
	ph7_value *pIdxKey;
	char *p,*ip = 0,*index;
	int bIsArray = 0,nNest = 0;
	/* php ignores leading SPACES in the name outright -- they are not mangled to
	 * '_' the way an interior space is. */
	while( zVar[0] == ' ' ){
		zVar++;
	}
	/* Neither a space nor a dot may live in a php variable name; both become '_'.
	 * The scan stops at the first '[', so only the BASE name is mangled. */
	for( p = zVar ; p[0] ; p++ ){
		if( p[0] == ' ' || p[0] == '.' ){
			p[0] = '_';
		}else if( p[0] == '[' ){
			bIsArray = 1;
			ip = p;
			p[0] = 0;
			break;
		}
	}
	if( p == zVar ){
		return; /* empty name (or a name that was nothing but a space) */
	}
	index = zVar;
	pIdxKey = ph7_context_new_scalar(pCtx);
	if( pIdxKey == 0 ){
		return;
	}
	while( bIsArray ){
		char *zSeg;
		ph7_hashmap *pNext;
		if( ++nNest > nMaxNest ){
			/* php drops the whole top-level variable it was building and warns.
			 * The message is deliberately vague about the input -- php calls
			 * saying more "information disclosure". */
			ph7_hashmap_node *pNode = 0;
			ph7_hashmap *pRoot = (ph7_hashmap *)pTarget->x.pOther;
			ph7_value_reset_string_cursor(pIdxKey);
			ph7_value_string(pIdxKey,zVar,(int)SyStrlen(zVar));
			if( PH7_HashmapLookup(pRoot,pIdxKey,&pNode) == SXRET_OK ){
				PH7_HashmapUnlinkNode(pNode,TRUE);
			}
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Input variable nesting level exceeded %d. To increase the limit "
				"change max_input_nesting_level in php.ini.",nMaxNest);
			return;
		}
		ip++;
		zSeg = ip;
		if( ip[0] == ' ' || ip[0] == '\t' || ip[0] == '\n' || ip[0] == '\r' ){
			ip++;   /* php skips ONE leading space before testing for ']' */
		}
		if( ip[0] == ']' ){
			zSeg = 0;   /* "[]" (and "[ ]") appends */
		}else{
			while( ip[0] && ip[0] != ']' ){ ip++; }
			if( ip[0] == 0 ){
				/* An unterminated '[': php un-terminates the name -- the bracket
				 * itself becomes '_' -- and the rest is mangled and used as a
				 * PLAIN key, so "a[b=1" registers "a_b". */
				zSeg[-1] = '_';
				for( p = zSeg ; p[0] ; p++ ){
					if( p[0] == ' ' || p[0] == '.' || p[0] == '[' ){
						p[0] = '_';
					}
				}
				break;
			}
			ip[0] = 0;
		}
		pNext = ParseStrDescend(pCtx,pCur,index,pIdxKey);
		if( pNext == 0 ){
			return;
		}
		pCur = pNext;
		index = zSeg;
		ip++;
		if( ip[0] == '[' ){
			ip[0] = 0;   /* another level follows */
		}else{
			break;       /* whatever trails the last ']' is ignored */
		}
	}
	if( index == 0 ){
		PH7_HashmapInsert(pCur,0,pVal);
	}else{
		ph7_value_reset_string_cursor(pIdxKey);
		ph7_value_string(pIdxKey,index,(int)SyStrlen(index));
		PH7_HashmapInsert(pCur,pIdxKey,pVal);
	}
}
/*
 * void parse_str(string $string, array &$result)
 *  Parse a query string into $result the way php's own GET/POST parser does.
 */
PH7_PRIVATE int PH7_builtin_parse_str(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pVal;
	SyBlob sSep,sName,sValue;
	const char *zIn,*zSep;
	int nByte,nSep;
	sxu32 i = 0;
	sxi64 nCount = 0,nMaxVars,nMaxNest;
	if( nArg < 2 ){
		/* Arity is enforced from aBuiltinSig[] before the call. */
		return PH7_OK;
	}
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	nMaxVars = PH7_VmIniGetInt(pCtx->pVm,"max_input_vars",1000);
	nMaxNest = PH7_VmIniGetInt(pCtx->pVm,"max_input_nesting_level",64);
	SyBlobInit(&sSep,&pCtx->pVm->sAllocator);
	SyBlobInit(&sName,&pCtx->pVm->sAllocator);
	SyBlobInit(&sValue,&pCtx->pVm->sAllocator);
	PH7_VmIniGetStr(pCtx->pVm,"arg_separator.input",&sSep);
	if( SyBlobLength(&sSep) < 1 ){
		SyBlobAppend(&sSep,"&",sizeof(char));
	}
	zSep = (const char *)SyBlobData(&sSep);
	nSep = (int)SyBlobLength(&sSep);
	/* php tokenizes with strtok(), so the separator is a SET of bytes and a run
	 * of them yields no empty field -- and an embedded NUL ends the input. */
	while( i < (sxu32)nByte && zIn[i] ){
		sxu32 iStart,iEq;
		int bFound;
		while( i < (sxu32)nByte && zIn[i] ){
			int s;
			for( s = 0 ; s < nSep ; ++s ){
				if( zIn[i] == zSep[s] ){ break; }
			}
			if( s == nSep ){ break; }
			i++;
		}
		if( i >= (sxu32)nByte || zIn[i] == 0 ){
			break;
		}
		iStart = i;
		iEq = 0;
		bFound = 0;
		while( i < (sxu32)nByte && zIn[i] ){
			int s;
			for( s = 0 ; s < nSep ; ++s ){
				if( zIn[i] == zSep[s] ){ break; }
			}
			if( s < nSep ){ break; }
			if( zIn[i] == '=' && !bFound ){
				iEq = i;
				bFound = 1;
			}
			i++;
		}
		if( ++nCount > nMaxVars ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Input variables exceeded %qd. To increase the limit change "
				"max_input_vars in php.ini.",nMaxVars);
			break;
		}
		/* Both halves are url-decoded BEFORE the name is parsed for brackets. */
		SyBlobReset(&sName);
		SyBlobReset(&sValue);
		if( bFound ){
			if( iEq > iStart ){
				SyUriDecode(&zIn[iStart],iEq - iStart,UriBlobConsumer,&sName,TRUE);
			}
			if( i > iEq + 1 ){
				SyUriDecode(&zIn[iEq + 1],i - (iEq + 1),UriBlobConsumer,&sValue,TRUE);
			}
		}else{
			SyUriDecode(&zIn[iStart],i - iStart,UriBlobConsumer,&sName,TRUE);
		}
		SyBlobAppend(&sName,"\0",sizeof(char));   /* the walk is C-string based */
		ph7_value_string(pVal,(const char *)SyBlobData(&sValue),(int)SyBlobLength(&sValue));
		ParseStrRegister(pCtx,pArray,(char *)SyBlobData(&sName),pVal,(int)nMaxNest);
		ph7_value_reset_string_cursor(pVal);
	}
	SyBlobRelease(&sSep);
	SyBlobRelease(&sName);
	SyBlobRelease(&sValue);
	/* $result is by REFERENCE and php REPLACES it, empty array included. */
	PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pArray);
	return PH7_OK;
}
/* --- http_build_query (php's ext/standard/http.c) ---------------------- */

/*
 * The chain of hashmaps and instances the walk is currently INSIDE. This is
 * php's GC_TRY_PROTECT_RECURSION without a mark bit: a container that is its own
 * ancestor contributes nothing, so `$a['self'] = &$a` builds "a=1" rather than
 * recursing forever. PHL had no guard here at all and ran the allocator out of
 * memory on exactly that input.
 */
typedef struct http_query_frame http_query_frame;
struct http_query_frame {
	const void *pWalked;                  /* the ph7_hashmap / ph7_class_instance */
	const http_query_frame *pParent;
};
typedef struct http_query_state http_query_state;
struct http_query_state {
	ph7_context *pCtx;
	SyBlob *pOut;      /* the form string built so far */
	const char *zSep;  /* argument separator */
	sxu32 nSep;
	int bRaw;          /* PHP_QUERY_RFC3986 rather than RFC1738 */
	int nDepth;
	int rc;            /* PH7_OK, or the status of a throw in flight */
};
/*
 * php has no fixed nesting limit here -- it asks the platform whether the C
 * stack is nearly gone and throws "Maximum call stack size reached." when it is.
 * PHL walks the same tree on the same C stack, so it needs a bound; this one is
 * far above any query string anyone builds and reports php's own error.
 */
#define HTTP_QUERY_MAX_DEPTH 512

static int HttpQueryIsAncestor(const http_query_frame *pFrame,const void *pWalked)
{
	while( pFrame ){
		if( pFrame->pWalked == pWalked ){
			return 1;
		}
		pFrame = pFrame->pParent;
	}
	return 0;
}
static void HttpQueryEncodeTo(SyBlob *pOut,int bRaw,const char *zIn,sxu32 nByte)
{
	if( nByte < 1 ){
		return;
	}
	if( bRaw ){
		SyUriEncodeRaw(zIn,nByte,UriBlobConsumer,pOut);
	}else{
		SyUriEncode(zIn,nByte,UriBlobConsumer,pOut);
	}
}
static int HttpQueryWalk(http_query_state *p,ph7_value *pData,
	const char *zNumPrefix,sxu32 nNumPrefix,
	const char *zKeyPrefix,sxu32 nKeyPrefix,
	const http_query_frame *pParent);

/*
 * php_url_encode_scalar(): one "<key_prefix><key>[%5D]=<value>" leaf, preceded
 * by the separator once anything has been written.
 */
static void HttpQueryScalar(http_query_state *p,
	int bIntKey,sxi64 iKey,const char *zKey,sxu32 nKey,
	ph7_value *pVal,
	const char *zNumPrefix,sxu32 nNumPrefix,
	const char *zKeyPrefix,sxu32 nKeyPrefix)
{
	if( SyBlobLength(p->pOut) > 0 ){
		SyBlobAppend(p->pOut,p->zSep,p->nSep);
	}
	if( nKeyPrefix > 0 ){
		SyBlobAppend(p->pOut,zKeyPrefix,nKeyPrefix);
	}
	if( bIntKey ){
		/* The numeric prefix is appended RAW -- php never url-encodes it, which
		 * is why http_build_query([1,2], "a b") answers "a b0=1&a b1=2". The
		 * chunk encoded it and answered "a+b0=1". */
		if( nNumPrefix > 0 ){
			SyBlobAppend(p->pOut,zNumPrefix,nNumPrefix);
		}
		SyBlobFormat(p->pOut,"%qd",iKey);
	}else{
		HttpQueryEncodeTo(p->pOut,p->bRaw,zKey,nKey);
	}
	if( nKeyPrefix > 0 ){
		SyBlobAppend(p->pOut,"%5D",sizeof("%5D")-1);
	}
	SyBlobAppend(p->pOut,"=",sizeof(char));
	if( ph7_value_is_bool(pVal) ){
		/* php writes the digit itself: to_string() would give "" for false. */
		SyBlobAppend(p->pOut,ph7_value_to_bool(pVal) ? "1" : "0",sizeof(char));
	}else{
		int nVal;
		const char *zVal = ph7_value_to_string(pVal,&nVal);
		HttpQueryEncodeTo(p->pOut,p->bRaw,zVal,(sxu32)nVal);
	}
}
/*
 * Build the key prefix a nested container's members carry: php closes the
 * PREVIOUS bracket and opens the next one in the same step, so a second level
 * appends "%5D%5B" where the first opened with "%5B".
 */
static void HttpQueryNestPrefix(http_query_state *p,SyBlob *pPrefix,
	int bIntKey,sxi64 iKey,const char *zKey,sxu32 nKey,
	const char *zNumPrefix,sxu32 nNumPrefix,
	const char *zKeyPrefix,sxu32 nKeyPrefix)
{
	if( nKeyPrefix > 0 ){
		SyBlobAppend(pPrefix,zKeyPrefix,nKeyPrefix);
	}else if( bIntKey && nNumPrefix > 0 ){
		SyBlobAppend(pPrefix,zNumPrefix,nNumPrefix);
	}
	if( bIntKey ){
		SyBlobFormat(pPrefix,"%qd",iKey);
	}else{
		HttpQueryEncodeTo(pPrefix,p->bRaw,zKey,nKey);
	}
	SyBlobAppend(pPrefix,nKeyPrefix > 0 ? "%5D%5B" : "%5B",
		nKeyPrefix > 0 ? sizeof("%5D%5B")-1 : sizeof("%5B")-1);
}
/*
 * One (key, value) pair, whichever container it came from. php skips NULL and
 * RESOURCE outright, descends into an array or a non-enum object, and treats
 * everything else -- a backed enum case included -- as a scalar.
 */
static void HttpQueryPair(http_query_state *p,
	int bIntKey,sxi64 iKey,const char *zKey,sxu32 nKey,
	ph7_value *pVal,
	const char *zNumPrefix,sxu32 nNumPrefix,
	const char *zKeyPrefix,sxu32 nKeyPrefix,
	const http_query_frame *pParent)
{
	int bDescend;
	if( p->rc != PH7_OK ){
		return;
	}
	if( ph7_value_is_null(pVal) || ph7_value_is_resource(pVal) ){
		return;
	}
	bDescend = ph7_value_is_array(pVal);
	if( ph7_value_is_object(pVal) ){
		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;
		if( (pInst->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){
			bDescend = 1;
		}else{
			/* php compares an enum case by its BACKING value here; the chunk
			 * descended into it and emitted its name/value properties. */
			ph7_value *pBacking = PH7_EnumCaseBackingValueOf(pInst);
			if( pBacking == 0 ){
				p->rc = PH7_VmThrowException(p->pCtx,"ValueError",
					"Unbacked enum %z cannot be converted to a string",
					&pInst->pClass->sName);
				return;
			}
			HttpQueryScalar(p,bIntKey,iKey,zKey,nKey,pBacking,
				zNumPrefix,nNumPrefix,zKeyPrefix,nKeyPrefix);
			return;
		}
	}
	if( bDescend ){
		SyBlob sPrefix;
		SyBlobInit(&sPrefix,&p->pCtx->pVm->sAllocator);
		HttpQueryNestPrefix(p,&sPrefix,bIntKey,iKey,zKey,nKey,
			zNumPrefix,nNumPrefix,zKeyPrefix,nKeyPrefix);
		/* php passes no numeric prefix down: it only ever prefixes a TOP-LEVEL
		 * integer key. */
		HttpQueryWalk(p,pVal,0,0,
			(const char *)SyBlobData(&sPrefix),SyBlobLength(&sPrefix),pParent);
		SyBlobRelease(&sPrefix);
		return;
	}
	HttpQueryScalar(p,bIntKey,iKey,zKey,nKey,pVal,
		zNumPrefix,nNumPrefix,zKeyPrefix,nKeyPrefix);
}
/* Every visible, non-static, materialized property of an instance, in
 * declaration order. php asks the CALLER's scope, so http_build_query($this)
 * from inside the class sees its private members -- the chunk reached them
 * through a global-scope get_object_vars() and never did. */
static void HttpQueryWalkObject(http_query_state *p,ph7_class_instance *pThis,
	const char *zKeyPrefix,sxu32 nKeyPrefix,const http_query_frame *pFrame)
{
	SyHashEntry *pEntry;
	ph7_value sValue;
	PH7_MemObjInit(pThis->pVm,&sValue);
	SyHashResetLoopCursor(&pThis->hAttr);
	while( p->rc == PH7_OK && (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){
		VmClassAttr *pAttr = (VmClassAttr *)pEntry->pUserData;
		SyString *pName = &pAttr->pAttr->sName;
		ph7_value *pValue;
		if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_HIDDEN) ){
			continue;
		}
		if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){
			/* A virtual hooked property has no backing store, and php reads the
			 * raw property table here rather than dispatching the get hook. */
			continue;
		}
		if( !PH7_VmClassMemberAccess(pThis->pVm,pThis->pClass,pName,
				pAttr->pAttr->iProtection,FALSE) ){
			continue;
		}
		pValue = PH7_ClassInstanceExtractAttrValue(pThis,pAttr);
		if( pValue == 0 ){
			continue;
		}
		PH7_MemObjLoad(pValue,&sValue);
		HttpQueryPair(p,0,0,SyStringData(pName),SyStringLength(pName),&sValue,
			0,0,zKeyPrefix,nKeyPrefix,pFrame);
		PH7_MemObjRelease(&sValue);
	}
	PH7_MemObjRelease(&sValue);
}
/* php_url_encode_hash_ex() over one array or object. */
static int HttpQueryWalk(http_query_state *p,ph7_value *pData,
	const char *zNumPrefix,sxu32 nNumPrefix,
	const char *zKeyPrefix,sxu32 nKeyPrefix,
	const http_query_frame *pParent)
{
	http_query_frame sFrame;
	const void *pWalked = pData->x.pOther;
	if( HttpQueryIsAncestor(pParent,pWalked) ){
		return PH7_OK;
	}
	if( p->nDepth >= HTTP_QUERY_MAX_DEPTH ){
		p->rc = PH7_VmThrowException(p->pCtx,"Error","Maximum call stack size reached.");
		return p->rc;
	}
	sFrame.pWalked = pWalked;
	sFrame.pParent = pParent;
	p->nDepth++;
	if( ph7_value_is_object(pData) ){
		HttpQueryWalkObject(p,(ph7_class_instance *)pWalked,zKeyPrefix,nKeyPrefix,&sFrame);
	}else{
		ph7_hashmap *pMap = (ph7_hashmap *)pWalked;
		ph7_hashmap_node *pNode = pMap->pFirst;
		ph7_value sValue;
		sxu32 n = pMap->nEntry;
		PH7_MemObjInit(pMap->pVm,&sValue);
		/* Insertion order runs pFirst then the pPrev chain (MACRO_LD_PUSH links
		 * a new node in through pNext, so pNext is the OLDER neighbour). */
		while( n > 0 && p->rc == PH7_OK ){
			int bIntKey = (pNode->iType == HASHMAP_INT_NODE);
			PH7_HashmapExtractNodeValue(pNode,&sValue,FALSE);
			HttpQueryPair(p,bIntKey,bIntKey ? pNode->xKey.iKey : 0,
				bIntKey ? 0 : (const char *)SyBlobData(&pNode->xKey.sKey),
				bIntKey ? 0 : SyBlobLength(&pNode->xKey.sKey),
				&sValue,zNumPrefix,nNumPrefix,zKeyPrefix,nKeyPrefix,&sFrame);
			PH7_MemObjRelease(&sValue);
			pNode = pNode->pPrev;
			n--;
		}
		PH7_MemObjRelease(&sValue);
	}
	p->nDepth--;
	return p->rc;
}
/*
 * string http_build_query(object|array $data, string $numeric_prefix = "",
 *                         ?string $arg_separator = null,
 *                         int $encoding_type = PHP_QUERY_RFC1738)
 *  Generate a URL-encoded query string from an array or an object.
 */
PH7_PRIVATE int PH7_builtin_http_build_query(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	http_query_state sState;
	SyBlob sOut;
	char zName[64];
	const char *zNumPrefix = 0,*zSep = "&";
	int nNumPrefix = 0,nSep = 1;
	if( nArg < 1 ){
		/* Arity is enforced from aBuiltinSig[] before the call. */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* php DECLARES `object|array $data` and REPORTS "must be of type array" --
	 * the shared ZPP screen leaves a union arm holding `array` alone for exactly
	 * this reason, so the wording is the builtin's own. */
	if( !ph7_value_is_array(apArg[0]) && !ph7_value_is_object(apArg[0]) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"http_build_query(): Argument #1 ($data) must be of type array, %s given",
			VmValueGivenName(apArg[0],zName,sizeof(zName)));
	}
	if( ph7_value_is_object(apArg[0]) ){
		ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;
		if( pInst->pClass->iFlags & PH7_CLASS_ENUM ){
			return PH7_VmThrowException(pCtx,"TypeError",
				"http_build_query(): Argument #1 ($data) must not be an enum, %z given",
				&pInst->pClass->sName);
		}
	}
	if( nArg > 1 ){
		zNumPrefix = ph7_value_to_string(apArg[1],&nNumPrefix);
	}
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
		zSep = ph7_value_to_string(apArg[2],&nSep);
	}
	sState.pCtx = pCtx;
	sState.zSep = zSep;
	sState.nSep = (sxu32)nSep;
	sState.bRaw = (nArg > 3) && (ph7_value_to_int(apArg[3]) == 2 /* PHP_QUERY_RFC3986 */);
	sState.nDepth = 0;
	sState.rc = PH7_OK;
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	sState.pOut = &sOut;
	HttpQueryWalk(&sState,apArg[0],zNumPrefix,(sxu32)nNumPrefix,0,0,0);
	if( sState.rc == PH7_OK ){
		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	}
	SyBlobRelease(&sOut);
	return sState.rc;
}
/*
 * string urldecode(string $str)
 *  Decodes any %## encoding in the given string.
 *  Plus symbols ('+') are decoded to a space character.
 * string rawurldecode(string $str)
 *  The same, except that '+' is NOT a space: RFC 3986 has no plus convention, so
 *  php leaves it alone. rawurldecode() used to be registered as an ALIAS of
 *  urldecode(), which turned every literal '+' into a space.
 * Parameter
 *  $data
 *    Input string.
 * Return
 *  Decoded URL or FALSE on failure.
 */
static int UrlDecodeCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPlus)
{
	const char *zIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the input string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* php returns an empty string for empty input, not FALSE */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Perform the URL decoding */
	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,bPlus);
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return UrlDecodeCommon(pCtx,nArg,apArg,TRUE);
}
PH7_PRIVATE int PH7_builtin_rawurldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return UrlDecodeCommon(pCtx,nArg,apArg,FALSE);
}
#endif /* PH7_NEED_BUILTIN_REG */
