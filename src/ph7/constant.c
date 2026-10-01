/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <float.h> /* DBL_EPSILON/DBL_MAX/DBL_MIN/DBL_DIG for the PHP_FLOAT_* constants */
#ifdef PH7_ENABLE_OPENSSL
/* The linked OpenSSL's own version, padding and purpose numbers: every one of
 * these is bound by SYMBOL rather than by value, so a build against another
 * 3.x answers that library's numbers exactly as php's build answers its. */
#include <openssl/opensslv.h>
#include <openssl/crypto.h>
#include <openssl/rsa.h>
#include <openssl/pkcs7.h>
#include <openssl/cms.h>
#include <openssl/x509v3.h>
#endif
#ifdef PH7_ENABLE_ZLIB
#include <zlib.h> /* ZLIB_VERSION/ZLIB_VERNUM: the library this build links */
#endif
#ifndef __WINNT__
/* ext/posix's constants are the platform's own macros -- see PH7_POSIX_*_Const. */
#include <unistd.h>
#include <sys/stat.h>
#include <sys/resource.h>
#endif
/* This file implement built-in constants for the PH7 engine. */
/*
 * PH7_VERSION
 * __PH7__
 *   Expand the current version of the PH7 engine.
 */
static void PH7_VER_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_string(pVal,ph7_lib_signature(),-1/*Compute length automatically*/);
}
/*
 * PHP_VERSION, PHP_MAJOR_VERSION, PHP_MINOR_VERSION, PHP_RELEASE_VERSION,
 * PHP_EXTRA_VERSION, PHP_VERSION_ID
 *   Expand the PHP-compatibility version PHL advertises (see PHP_COMPAT_* in ph7.h).
 */
static void PH7_PHPVerConst(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_string(pVal,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION)-1);
}
static void PH7_PHPMajorConst(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int64(pVal,PHP_COMPAT_MAJOR_VERSION);
}
static void PH7_PHPMinorConst(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int64(pVal,PHP_COMPAT_MINOR_VERSION);
}
static void PH7_PHPReleaseConst(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int64(pVal,PHP_COMPAT_RELEASE_VERSION);
}
static void PH7_PHPExtraConst(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_string(pVal,PHP_COMPAT_EXTRA_VERSION,(int)sizeof(PHP_COMPAT_EXTRA_VERSION)-1);
}
static void PH7_PHPVerIdConst(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int64(pVal,PHP_COMPAT_VERSION_ID);
}
#ifdef __WINNT__
#include <Windows.h>
#elif defined(__UNIXES__)
#include <sys/utsname.h>
#endif
/*
 * PHP_OS
 *  Expand the name of the host Operating System.
 */
static void PH7_OS_Const(ph7_value *pVal,void *pUnused)
{
#if defined(__WINNT__)
	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);
#elif defined(__UNIXES__)
	struct utsname sInfo;
	if( uname(&sInfo) != 0 ){
		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);
	}else{
		ph7_value_string(pVal,sInfo.sysname,-1);
	}
#else
	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);
#endif
	SXUNUSED(pUnused);
}
/*
 * PHP_OS_FAMILY (php 7.2)
 *  One of 'Windows', 'BSD', 'Darwin', 'Solaris', 'Linux' or 'Unknown', derived
 *  from the host's uname sysname (php maps the same set at build time).
 */
static void PH7_OS_FAMILY_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
#if defined(__WINNT__)
	ph7_value_string(pVal,"Windows",(int)sizeof("Windows")-1);
#elif defined(__UNIXES__)
	struct utsname sInfo;
	const char *zFamily = "Unknown";
	if( uname(&sInfo) == 0 ){
		const char *z = sInfo.sysname;
		if( SyStrnicmp(z,"Darwin",sizeof("Darwin")-1) == 0 ){
			zFamily = "Darwin";
		}else if( SyStrnicmp(z,"Linux",sizeof("Linux")-1) == 0 ){
			zFamily = "Linux";
		}else if( SyStrnicmp(z,"SunOS",sizeof("SunOS")-1) == 0 ){
			zFamily = "Solaris";
		}else{
			/* FreeBSD/OpenBSD/NetBSD/DragonFly -> 'BSD' (scan for "BSD"). */
			const char *p = z;
			while( p[0] && p[1] && p[2] ){
				if( (p[0]=='B'||p[0]=='b') && (p[1]=='S'||p[1]=='s') && (p[2]=='D'||p[2]=='d') ){
					zFamily = "BSD";
					break;
				}
				p++;
			}
		}
	}
	ph7_value_string(pVal,zFamily,-1);
#else
	ph7_value_string(pVal,"Unknown",(int)sizeof("Unknown")-1);
#endif
}
/*
 * PHP_SAPI
 *  The interface between the interpreter and the host. PHL's host binary is a
 *  command-line interpreter, so this is "cli" (matching the CLI default of
 *  php_sapi_name(); the built-in -S server's per-request "cli-server" flavour is
 *  only surfaced by php_sapi_name(), not this compile-time constant).
 */
static void PH7_SAPI_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_string(pVal,"cli",(int)sizeof("cli")-1);
}
/*
 * PHP_EOL
 *  Expand the correct 'End Of Line' symbol for this platform.
 */
static void PH7_EOL_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
#ifdef __WINNT__
	ph7_value_string(pVal,"\r\n",(int)sizeof("\r\n")-1);
#else
	ph7_value_string(pVal,"\n",(int)sizeof(char));
#endif
}
/*
 * PHP_INT_MAX
 * Expand the largest integer supported.
 * Note that PH7 deals with 64-bit integer for all platforms.
 */
static void PH7_INTMAX_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int64(pVal,SXI64_HIGH);
}
/*
 * ext/calendar: the four calendars, numbered in the order the conversion table
 * holds them, and CAL_NUM_CALS as their count -- which is what makes the
 * "valid calendar ID" screen a plain 0 <= id < CAL_NUM_CALS test.
 */
static void PH7_CAL_GREGORIAN_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,0);
}
static void PH7_CAL_JULIAN_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,1);
}
static void PH7_CAL_JEWISH_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,2);
}
static void PH7_CAL_FRENCH_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,3);
}
static void PH7_CAL_NUM_CALS_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,4);
}
/*
 * easter_days()/easter_date()'s $mode. DEFAULT is not a rule but a
 * date-dependent choice between the two below it; ROMAN moves the 1583-1752
 * window to the Gregorian rule, and the two ALWAYS_ modes pin one rule for
 * every year.
 */
static void PH7_CAL_EASTER_DEFAULT_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,0);
}
static void PH7_CAL_EASTER_ROMAN_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,1);
}
static void PH7_CAL_EASTER_ALWAYS_GREGORIAN_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,2);
}
static void PH7_CAL_EASTER_ALWAYS_JULIAN_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,3);
}
/* jddayofweek()'s three modes. Note that SHORT is 2 and LONG is 1: the numbers
 * are not in the order the names suggest. */
static void PH7_CAL_DOW_DAYNO_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,0);
}
static void PH7_CAL_DOW_LONG_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,1);
}
static void PH7_CAL_DOW_SHORT_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,2);
}
/* jdmonthname()'s six modes, which pick the CALENDAR as well as the spelling
 * and are numbered independently of the CAL_* calendar ids above. */
static void PH7_CAL_MONTH_GREGORIAN_SHORT_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,0);
}
static void PH7_CAL_MONTH_GREGORIAN_LONG_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,1);
}
static void PH7_CAL_MONTH_JULIAN_SHORT_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,2);
}
static void PH7_CAL_MONTH_JULIAN_LONG_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,3);
}
static void PH7_CAL_MONTH_JEWISH_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,4);
}
static void PH7_CAL_MONTH_FRENCH_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,5);
}
/*
 * ext/calendar: the three flags jdtojewish()'s Hebrew spelling reads. They are
 * a bit set, so a caller may ask for any combination of them.
 */
static void PH7_CAL_JEWISH_ADD_ALAFIM_GERESH_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,2);
}
static void PH7_CAL_JEWISH_ADD_ALAFIM_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,4);
}
static void PH7_CAL_JEWISH_ADD_GERESHAYIM_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,8);
}
/*
 * ext/standard's IMAGETYPE_* space (php's image_filetype enum), in php's own
 * numbering. Three of the names are not enum members at all:
 * IMAGETYPE_JPEG2000 is a userland ALIAS for IMAGETYPE_JPC (9, the raw
 * codestream) rather than a type of its own, IMAGETYPE_UNKNOWN is the zero
 * every detection ladder falls out at, and IMAGETYPE_COUNT is the number of
 * types -- which is the FIXED count plus one for every handler a build
 * registers, so the SVG reader ext/libxml installs makes it 22 instead of 21.
 */
#define PH7_IMAGETYPE_CONST(fn,v)                     \
	static void fn(ph7_value *pVal,void *pUnused)     \
	{                                                 \
		SXUNUSED(pUnused);                            \
		ph7_value_int(pVal,v);                        \
	}
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_UNKNOWN_Const,   0)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_GIF_Const,       1)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JPEG_Const,      2)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_PNG_Const,       3)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_SWF_Const,       4)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_PSD_Const,       5)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_BMP_Const,       6)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_TIFF_II_Const,   7)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_TIFF_MM_Const,   8)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JPC_Const,       9)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JP2_Const,      10)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JPX_Const,      11)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JB2_Const,      12)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_SWC_Const,      13)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_IFF_Const,      14)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_WBMP_Const,     15)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_XBM_Const,      16)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_ICO_Const,      17)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_WEBP_Const,     18)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_AVIF_Const,     19)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_HEIF_Const,     20)
#ifdef PH7_ENABLE_LIBXML
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_SVG_Const,      21)
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_COUNT_Const,    22)
#else
PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_COUNT_Const,    21)
#endif
/*
 * PHP_INT_MIN (php 7.0)
 * Expand the smallest integer supported.
 */
static void PH7_INTMIN_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int64(pVal,SMALLEST_INT64);
}
/*
 * PHP_INT_SIZE
 * Expand the size in bytes of a 64-bit integer.
 */
static void PH7_INTSIZE_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int64(pVal,sizeof(sxi64));
}
/*
 * PHP_FLOAT_EPSILON / PHP_FLOAT_MAX / PHP_FLOAT_MIN / PHP_FLOAT_DIG (php 7.2)
 * Double-precision characteristics, sourced from <float.h> exactly like php
 * so they track the compiling platform's actual double representation.
 */
static void PH7_FLOATEPSILON_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_double(pVal,DBL_EPSILON);
}
static void PH7_FLOATMAX_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_double(pVal,DBL_MAX);
}
static void PH7_FLOATMIN_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_double(pVal,DBL_MIN);
}
static void PH7_FLOATDIG_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int64(pVal,DBL_DIG);
}
/*
 * DIRECTORY_SEPARATOR.
 * Expand the directory separator character.
 */
static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
#ifdef __WINNT__
	ph7_value_string(pVal,"\\",(int)sizeof(char));
#else
	ph7_value_string(pVal,"/",(int)sizeof(char));
#endif
}
/*
 * PATH_SEPARATOR.
 * Expand the path separator character.
 */
static void PH7_PATHSEP_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
#ifdef __WINNT__
	ph7_value_string(pVal,";",(int)sizeof(char));
#else
	ph7_value_string(pVal,":",(int)sizeof(char));
#endif
}

#if defined(PH7_ENABLE_MATH_FUNC)
/*
 * NAN constant: floating-point Not-A-Number
 */
static void PH7_NAN_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_double(pVal, PH7_NAN_VALUE());
}

/*
 * INF constant: positive infinity
 */
static void PH7_INF_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	/* similarly avoid the INFINITY macro */
	ph7_value_double(pVal, PH7_INF_VALUE());
}
#endif /* PH7_ENABLE_MATH_FUNC */

#ifndef __WINNT__
#include <time.h>
#endif
/*
 * __TIME__
 *  Expand the current time (GMT).
 */
static void PH7_TIME_Const(ph7_value *pVal,void *pUnused)
{
	Sytm sTm;
#ifdef __WINNT__
	SYSTEMTIME sOS;
	GetSystemTime(&sOS);
	SYSTEMTIME_TO_SYTM(&sOS,&sTm);
#else
	struct tm *pTm;
	time_t t;
	time(&t);
	pTm = gmtime(&t);
	STRUCT_TM_TO_SYTM(pTm,&sTm);
#endif
	SXUNUSED(pUnused); /* cc warning */
	/* Expand */
	ph7_value_string_format(pVal,"%02d:%02d:%02d",sTm.tm_hour,sTm.tm_min,sTm.tm_sec);
}
/*
 * __DATE__
 *  Expand the current date in the ISO-8601 format.
 */
static void PH7_DATE_Const(ph7_value *pVal,void *pUnused)
{
	Sytm sTm;
#ifdef __WINNT__
	SYSTEMTIME sOS;
	GetSystemTime(&sOS);
	SYSTEMTIME_TO_SYTM(&sOS,&sTm);
#else
	struct tm *pTm;
	time_t t;
	time(&t);
	pTm = gmtime(&t);
	STRUCT_TM_TO_SYTM(pTm,&sTm);
#endif
	SXUNUSED(pUnused); /* cc warning */
	/* Expand */
	ph7_value_string_format(pVal,"%04qd-%02d-%02d",sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);
}
/*
 * __FILE__
 *  Path of the processed script.
 */
static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_vm *pVm = (ph7_vm *)pUserData;
	SyString *pFile;
	/* The unit the LITERAL is written in, which php fixes at compile time: the
	 * declared file of the function running, else the unit on top of the include
	 * stack. Reading the stack top alone answered the unit currently being LOADED,
	 * so a function defined in one file and called from an include (or from an
	 * eval()'d chunk, whose own name is now such an entry) reported the caller's
	 * file as its own. */
	pFile = PH7_VmExecutingUnitFile(&(*pVm));
	if( pFile == 0 ){
		/* Expand the magic word: ":MEMORY:" */
		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);
	}else{
		ph7_value_string(pVal,pFile->zString,pFile->nByte);
	}
}
/*
 * __DIR__
 *  Directory holding the processed script.
 */
static void PH7_DIR_Const(ph7_value *pVal,void *pUserData)
{
	ph7_vm *pVm = (ph7_vm *)pUserData;
	SyString *pFile;
	/* Same question as __FILE__, one directory up. */
	pFile = PH7_VmExecutingUnitFile(&(*pVm));
	if( pFile == 0 ){
		/* Expand the magic word: ":MEMORY:" */
		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);
	}else{
		if( pFile->nByte > 0 ){
			const char *zDir;
			int nLen;
			zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);
			ph7_value_string(pVal,zDir,nLen);
		}else{
			/* Expand '.' as the current directory*/
			ph7_value_string(pVal,".",(int)sizeof(char));
		}
	}
}
/*
 * PHP_SHLIB_SUFFIX
 *  Expand shared library suffix.
 */
static void PH7_PHP_SHLIB_SUFFIX_Const(ph7_value *pVal,void *pUserData)
{
#ifdef __WINNT__
	ph7_value_string(pVal,"dll",(int)sizeof("dll")-1);
#else
	ph7_value_string(pVal,"so",(int)sizeof("so")-1);
#endif
	SXUNUSED(pUserData); /* cc warning */
}
/*
 * E_ERROR
 *  Expands 1
 */
static void PH7_E_ERROR_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,1);
	SXUNUSED(pUserData);
}
/*
 * E_WARNING
 *  Expands 2
 */
static void PH7_E_WARNING_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,2);
	SXUNUSED(pUserData);
}
/*
 * E_PARSE
 *  Expands 4
 */
static void PH7_E_PARSE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,4);
	SXUNUSED(pUserData);
}
/*
 * E_NOTICE
 * Expands 8
 */
static void PH7_E_NOTICE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,8);
	SXUNUSED(pUserData);
}
/*
 * E_CORE_ERROR
 * Expands 16
 */
static void PH7_E_CORE_ERROR_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,16);
	SXUNUSED(pUserData);
}
/*
 * E_CORE_WARNING
 * Expands 32
 */
static void PH7_E_CORE_WARNING_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,32);
	SXUNUSED(pUserData);
}
/*
 * E_COMPILE_ERROR
 * Expands 64
 */
static void PH7_E_COMPILE_ERROR_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,64);
	SXUNUSED(pUserData);
}
/*
 * E_COMPILE_WARNING
 * Expands 128
 */
static void PH7_E_COMPILE_WARNING_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,128);
	SXUNUSED(pUserData);
}
/*
 * E_USER_ERROR
 * Expands 256
 */
static void PH7_E_USER_ERROR_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,256);
	SXUNUSED(pUserData);
}
/*
 * E_USER_WARNING
 * Expands 512
 */
static void PH7_E_USER_WARNING_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,512);
	SXUNUSED(pUserData);
}
/*
 * E_USER_NOTICE
 * Expands 1024
 */
static void PH7_E_USER_NOTICE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,1024);
	SXUNUSED(pUserData);
}
/*
 * E_RECOVERABLE_ERROR
 * Expands 4096
 */
static void PH7_E_RECOVERABLE_ERROR_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,4096);
	SXUNUSED(pUserData);
}
/*
 * E_STRICT
 * Expands 2048. php 8.4 removed the error LEVEL but kept the constant, marked
 * deprecated -- a program may still name it (and still gets 2048), it just says
 * so. It is deliberately NOT part of E_ALL, which is why PH7_E_ALL_MASK is
 * 30719 and not 32767.
 */
static void PH7_E_STRICT_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,2048);
	SXUNUSED(pUserData);
}
/*
 * E_DEPRECATED
 * Expands 8192
 */
static void PH7_E_DEPRECATED_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,8192);
	SXUNUSED(pUserData);
}
/*
 * E_USER_DEPRECATED
 *   Expands 16384.
 */
static void PH7_E_USER_DEPRECATED_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,16384);
	SXUNUSED(pUserData);
}
/*
 * E_ALL
 *  Expands 30719 (php 8: E_STRICT is no longer part of E_ALL)
 */
static void PH7_E_ALL_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_E_ALL_MASK);
	SXUNUSED(pUserData);
}
/*
 * CASE_LOWER
 *  Expands 0.
 */
static void PH7_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,0);
	SXUNUSED(pUserData);
}
/*
 * CASE_UPPER
 *  Expands 1.
 */
static void PH7_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,1);
	SXUNUSED(pUserData);
}
/*
 * STR_PAD_LEFT
 *  Expands 0.
 */
static void PH7_STR_PAD_LEFT_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,0);
	SXUNUSED(pUserData);
}
/*
 * STR_PAD_RIGHT
 *  Expands 1.
 */
static void PH7_STR_PAD_RIGHT_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,1);
	SXUNUSED(pUserData);
}
/*
 * STR_PAD_BOTH
 *  Expands 2.
 */
static void PH7_STR_PAD_BOTH_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,2);
	SXUNUSED(pUserData);
}
/*
 * stream_wrapper_register()'s $flags. php defines exactly this one bit: the
 * wrapper speaks to the network, so allow_url_fopen gates opening it and
 * allow_url_include gates INCLUDING it.
 */
static void PH7_STREAM_IS_URL_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_IS_URL);
	SXUNUSED(pUserData);
}
/*
 * The rest of the streamWrapper PROTOCOL vocabulary: the numbers php hands a
 * userland wrapper, and the ones a wrapper hands back. PHL registered wrappers
 * without them, so the ordinary spellings every real wrapper is written against --
 * `$options & STREAM_USE_PATH` in stream_open(), `$flags & STREAM_URL_STAT_QUIET`
 * in url_stat(), the STREAM_META_* verb in stream_metadata() -- were an undefined
 * constant, i.e. an Error, in code php runs.
 */
static void PH7_STREAM_USE_PATH_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_USE_PATH);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_IGNORE_URL_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_IGNORE_URL);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_REPORT_ERRORS_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_REPORT_ERRORS);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_MUST_SEEK_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_MUST_SEEK);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_URL_STAT_LINK_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_URL_STAT_LINK);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_URL_STAT_QUIET_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_URL_STAT_QUIET);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_MKDIR_RECURSIVE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_MKDIR_RECURSIVE);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_META_TOUCH_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_META_TOUCH);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_META_OWNER_NAME_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_META_OWNER_NAME);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_META_OWNER_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_META_OWNER);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_META_GROUP_NAME_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_META_GROUP_NAME);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_META_GROUP_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_META_GROUP);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_META_ACCESS_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_META_ACCESS);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_OPTION_BLOCKING_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_OPTION_BLOCKING);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_OPTION_READ_BUFFER_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_OPTION_READ_BUFFER);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_OPTION_WRITE_BUFFER_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_OPTION_WRITE_BUFFER);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_OPTION_READ_TIMEOUT_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_OPTION_READ_TIMEOUT);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_BUFFER_NONE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_BUFFER_NONE);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_BUFFER_LINE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_BUFFER_LINE);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_BUFFER_FULL_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_BUFFER_FULL);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_CAST_AS_STREAM_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_CAST_AS_STREAM);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_CAST_FOR_SELECT_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_CAST_FOR_SELECT);
	SXUNUSED(pUserData);
}
/*
 * A userland filter's ANSWER, and which kind of call it is answering. FEED_ME
 * says "I produced nothing, ask me again with more"; ERR_FATAL ends the stream.
 */
static void PH7_PSFS_PASS_ON_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_PSFS_PASS_ON);
	SXUNUSED(pUserData);
}
static void PH7_PSFS_FEED_ME_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_PSFS_FEED_ME);
	SXUNUSED(pUserData);
}
static void PH7_PSFS_ERR_FATAL_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_PSFS_ERR_FATAL);
	SXUNUSED(pUserData);
}
static void PH7_PSFS_FLAG_NORMAL_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_PSFS_FLAG_NORMAL);
	SXUNUSED(pUserData);
}
static void PH7_PSFS_FLAG_FLUSH_INC_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_PSFS_FLAG_FLUSH_INC);
	SXUNUSED(pUserData);
}
static void PH7_PSFS_FLAG_FLUSH_CLOSE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_PSFS_FLAG_FLUSH_CLOSE);
	SXUNUSED(pUserData);
}
/*
 * The `notification` callback's first argument: WHICH event the stream layer is
 * reporting. php defines all ten whatever its build registered, so a script may
 * name one no wrapper here raises -- an unmatched `case` is silent where a
 * missing constant is a fatal.
 */
static void PH7_STREAM_NOTIFY_RESOLVE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_STREAM_NOTIFY_RESOLVE);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_NOTIFY_CONNECT_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_STREAM_NOTIFY_CONNECT);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_NOTIFY_AUTH_REQUIRED_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_STREAM_NOTIFY_AUTH_REQUIRED);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_NOTIFY_MIME_TYPE_IS_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_STREAM_NOTIFY_MIME_TYPE_IS);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_NOTIFY_FILE_SIZE_IS_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_STREAM_NOTIFY_FILE_SIZE_IS);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_NOTIFY_REDIRECTED_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_STREAM_NOTIFY_REDIRECTED);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_NOTIFY_PROGRESS_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_STREAM_NOTIFY_PROGRESS);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_NOTIFY_COMPLETED_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_STREAM_NOTIFY_COMPLETED);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_NOTIFY_FAILURE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_STREAM_NOTIFY_FAILURE);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_NOTIFY_AUTH_RESULT_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_STREAM_NOTIFY_AUTH_RESULT);
	SXUNUSED(pUserData);
}
/* And the callback's second argument: how bad the event is. */
static void PH7_STREAM_NOTIFY_SEVERITY_INFO_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_STREAM_NOTIFY_SEVERITY_INFO);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_NOTIFY_SEVERITY_WARN_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_STREAM_NOTIFY_SEVERITY_WARN);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_NOTIFY_SEVERITY_ERR_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_STREAM_NOTIFY_SEVERITY_ERR);
	SXUNUSED(pUserData);
}
/*
 * stream_filter_append()'s $mode — WHICH chain the filter joins. php's 0 is not
 * "neither": it means "whichever chains the handle's own mode makes sense for".
 */
static void PH7_STREAM_FILTER_READ_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_STREAM_FILTER_READ);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_FILTER_WRITE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_STREAM_FILTER_WRITE);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_FILTER_ALL_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PHL_STREAM_FILTER_ALL);
	SXUNUSED(pUserData);
}
/*
 * stream_socket_client()'s $flags. CONNECT is the default it documents;
 * PERSISTENT is what pfsockopen() means and the only one that changes what a
 * second call to the same address ANSWERS.
 */
static void PH7_STREAM_CLIENT_CONNECT_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_CLIENT_CONNECT);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_CLIENT_ASYNC_CONNECT_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_CLIENT_ASYNC_CONNECT);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_CLIENT_PERSISTENT_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_CLIENT_PERSISTENT);
	SXUNUSED(pUserData);
}
/*
 * The socket-family constants. Their VALUES are the platform's own — AF_INET6 is
 * 10 on Linux, 23 on Windows and 30 on the BSDs — so they are asked for by id
 * rather than written down here, and a program handing one to
 * stream_socket_pair() is handing the OS its own number.
 */
#ifdef PH7_ENABLE_NET
static void PH7_STREAM_PF_INET_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_PF_INET));
	SXUNUSED(pUserData);
}
static void PH7_STREAM_PF_INET6_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_PF_INET6));
	SXUNUSED(pUserData);
}
static void PH7_STREAM_PF_UNIX_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_PF_UNIX));
	SXUNUSED(pUserData);
}
static void PH7_STREAM_SOCK_STREAM_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_STREAM));
	SXUNUSED(pUserData);
}
static void PH7_STREAM_SOCK_DGRAM_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_DGRAM));
	SXUNUSED(pUserData);
}
static void PH7_STREAM_SOCK_RAW_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_RAW));
	SXUNUSED(pUserData);
}
static void PH7_STREAM_SOCK_SEQPACKET_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_SEQPACKET));
	SXUNUSED(pUserData);
}
static void PH7_STREAM_SOCK_RDM_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_RDM));
	SXUNUSED(pUserData);
}
static void PH7_STREAM_IPPROTO_IP_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_IP));
	SXUNUSED(pUserData);
}
static void PH7_STREAM_IPPROTO_TCP_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_TCP));
	SXUNUSED(pUserData);
}
static void PH7_STREAM_IPPROTO_UDP_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_UDP));
	SXUNUSED(pUserData);
}
static void PH7_STREAM_IPPROTO_ICMP_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_ICMP));
	SXUNUSED(pUserData);
}
static void PH7_STREAM_IPPROTO_RAW_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_RAW));
	SXUNUSED(pUserData);
}
#endif /* PH7_ENABLE_NET */
/*
 * stream_socket_shutdown()'s $mode, and the two recvfrom/sendto flags. These
 * three ARE php's own numbers rather than the OS's: php maps STREAM_OOB and
 * STREAM_PEEK onto MSG_OOB/MSG_PEEK itself.
 */
static void PH7_STREAM_SHUT_RD_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_SHUT_RD);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_SHUT_WR_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_SHUT_WR);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_SHUT_RDWR_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_SHUT_RDWR);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_OOB_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_OOB);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_PEEK_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_PEEK);
	SXUNUSED(pUserData);
}
/*
 * stream_socket_server()'s $flags. Its default is BIND|LISTEN, and the two are
 * separate because binding is all a datagram server does.
 */
static void PH7_STREAM_SERVER_BIND_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_SERVER_BIND);
	SXUNUSED(pUserData);
}
static void PH7_STREAM_SERVER_LISTEN_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_STREAM_SERVER_LISTEN);
	SXUNUSED(pUserData);
}
/*
 * mt_srand()'s $mode: which GENERATOR to seed. MT_RAND_PHP is php's pre-7.1
 * Mersenne Twister, whose twist reads the low bit of the wrong word — a
 * different sequence, which is the only reason to ask for it.
 */
static void PH7_MT_RAND_MT19937_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_MT_RAND_MT19937);
	SXUNUSED(pUserData);
}
static void PH7_MT_RAND_PHP_Const(ph7_value *pVal,void *pUserData)
{
	/* php 8.3 deprecated the SYMBOL as well as the mode. The notice is
	 * aDeprecatedConst[]'s now, raised where every deprecated constant's is. */
	ph7_value_int(pVal,PH7_MT_RAND_PHP);
	SXUNUSED(pUserData);
}
/*
 * Output-handler flags and phases (ob_start()'s $flags, and the $phase an output
 * handler is called with). The values are php's and are a public ABI: the phase
 * bits are OR'd together (a first FLUSH arrives as FLUSH|START = 5), and the
 * three capability flags are the ones ob_clean()/ob_flush()/ob_end_*() test
 * before they will touch the buffer.
 */
static void PH7_OB_WRITE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_OB_WRITE);
	SXUNUSED(pUserData);
}
static void PH7_OB_START_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_OB_START);
	SXUNUSED(pUserData);
}
static void PH7_OB_CLEAN_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_OB_CLEAN);
	SXUNUSED(pUserData);
}
static void PH7_OB_FLUSH_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_OB_FLUSH);
	SXUNUSED(pUserData);
}
static void PH7_OB_FINAL_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_OB_FINAL);
	SXUNUSED(pUserData);
}
static void PH7_OB_CLEANABLE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_OB_CLEANABLE);
	SXUNUSED(pUserData);
}
static void PH7_OB_FLUSHABLE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_OB_FLUSHABLE);
	SXUNUSED(pUserData);
}
static void PH7_OB_REMOVABLE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_OB_REMOVABLE);
	SXUNUSED(pUserData);
}
static void PH7_OB_STDFLAGS_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_OB_STDFLAGS);
	SXUNUSED(pUserData);
}
static void PH7_OB_STARTED_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_OB_STARTED);
	SXUNUSED(pUserData);
}
static void PH7_OB_DISABLED_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_OB_DISABLED);
	SXUNUSED(pUserData);
}
static void PH7_OB_PROCESSED_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,PH7_OB_PROCESSED);
	SXUNUSED(pUserData);
}
/*
 * array_filter()'s $mode selector. The VALUES are php's and are a public ABI --
 * ARRAY_FILTER_USE_BOTH is 1 and ARRAY_FILTER_USE_KEY is 2, NOT the other way
 * round, and they are a selector rather than a bit mask (php reads the argument
 * with ==, so any other number is the default value mode).
 */
/*
 * CONNECTION_NORMAL / CONNECTION_ABORTED / CONNECTION_TIMEOUT
 *  The three states connection_status() reports. On a CLI there is no client
 *  to disconnect and no time limit to run out, so NORMAL is the only one that
 *  is ever answered -- but the names are what a program COMPARES against, and
 *  an undefined constant is a fatal.
 */
static void PH7_CONNECTION_NORMAL_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,0);
	SXUNUSED(pUserData);
}
static void PH7_CONNECTION_ABORTED_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,1);
	SXUNUSED(pUserData);
}
static void PH7_CONNECTION_TIMEOUT_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,2);
	SXUNUSED(pUserData);
}
static void PH7_ARRAY_FILTER_USE_KEY_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,2);
	SXUNUSED(pUserData);
}
static void PH7_ARRAY_FILTER_USE_BOTH_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,1);
	SXUNUSED(pUserData);
}
/*
 * COUNT_NORMAL
 *  Expands 0
 */
static void PH7_COUNT_NORMAL_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,0);
	SXUNUSED(pUserData);
}
/*
 * COUNT_RECURSIVE
 *  Expands 1.
 */
static void PH7_COUNT_RECURSIVE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,1);
	SXUNUSED(pUserData);
}
/*
 * php's sort-flag constants. The VALUES must match php exactly: they are a
 * public ABI (code passes literal ints, dumps them, and OR-combines the base
 * type with SORT_FLAG_CASE). SORT_ASC/SORT_DESC are the array_multisort
 * direction flags.
 * SORT_REGULAR 0 · SORT_NUMERIC 1 · SORT_STRING 2 · SORT_DESC 3 · SORT_ASC 4 ·
 * SORT_LOCALE_STRING 5 · SORT_NATURAL 6 · SORT_FLAG_CASE 8
 */
static void PH7_SORT_ASC_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,4);
	SXUNUSED(pUserData);
}
static void PH7_SORT_DESC_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,3);
	SXUNUSED(pUserData);
}
static void PH7_SORT_REG_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,0);
	SXUNUSED(pUserData);
}
static void PH7_SORT_NUMERIC_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,1);
	SXUNUSED(pUserData);
}
static void PH7_SORT_STRING_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,2);
	SXUNUSED(pUserData);
}
static void PH7_SORT_LOCALE_STRING_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,5);
	SXUNUSED(pUserData);
}
static void PH7_SORT_NATURAL_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,6);
	SXUNUSED(pUserData);
}
static void PH7_SORT_FLAG_CASE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,8);
	SXUNUSED(pUserData);
}
/*
 * PHP_ROUND_HALF_UP
 *  Expands 1.
 */
static void PH7_PHP_ROUND_HALF_UP_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,1);
	SXUNUSED(pUserData);
}
/*
 * PHP_SESSION_DISABLED / PHP_SESSION_NONE / PHP_SESSION_ACTIVE
 *  session_status() states (0 / 1 / 2).
 */
static void PH7_PHP_SESSION_DISABLED_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,0);
	SXUNUSED(pUserData);
}
static void PH7_PHP_SESSION_NONE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,1);
	SXUNUSED(pUserData);
}
static void PH7_PHP_SESSION_ACTIVE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,2);
	SXUNUSED(pUserData);
}
/*
 * INI_USER / INI_PERDIR / INI_SYSTEM / INI_ALL
 *  php.ini access levels (1 / 2 / 4 / 7).
 */
static void PH7_INI_USER_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,1);
	SXUNUSED(pUserData);
}
static void PH7_INI_PERDIR_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,2);
	SXUNUSED(pUserData);
}
static void PH7_INI_SYSTEM_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,4);
	SXUNUSED(pUserData);
}
static void PH7_INI_ALL_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,7);
	SXUNUSED(pUserData);
}
/*
 * MB_CASE_UPPER / MB_CASE_LOWER / MB_CASE_TITLE (0 / 1 / 2)
 */
static void PH7_MB_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,0);
	SXUNUSED(pUserData);
}
static void PH7_MB_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,1);
	SXUNUSED(pUserData);
}
static void PH7_MB_CASE_TITLE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,2);
	SXUNUSED(pUserData);
}
/*
 * SPHP_ROUND_HALF_DOWN
 *  Expands 2.
 */
static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,2);
	SXUNUSED(pUserData);
}
/*
 * PHP_ROUND_HALF_EVEN
 *  Expands 3.
 */
static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,3);
	SXUNUSED(pUserData);
}
/*
 * PHP_ROUND_HALF_ODD
 *  Expands 4.
 */
static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,4);
	SXUNUSED(pUserData);
}
/*
 * DEBUG_BACKTRACE_PROVIDE_OBJECT
 *  Expand 0x01
 * NOTE:
 *  The expanded value must be a power of two.
 */
static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */
	SXUNUSED(pUserData);
}
/*
 * DEBUG_BACKTRACE_IGNORE_ARGS
 *  Expand 0x02
 * NOTE:
 *  The expanded value must be a power of two.
 */
static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */
	SXUNUSED(pUserData);
}
#ifdef PH7_ENABLE_MATH_FUNC
/*
 * M_PI
 *  Expand the value of pi.
 */
static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,PH7_PI);
}
/*
 * M_E
 *  Expand 2.7182818284590452354
 */
static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,2.7182818284590452354);
}
/*
 * M_LOG2E
 *  Expand 2.7182818284590452354
 */
static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,1.4426950408889634074);
}
/*
 * M_LOG10E
 *  Expand 0.4342944819032518276
 */
static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,0.4342944819032518276);
}
/*
 * M_LN2
 *  Expand 	0.69314718055994530942
 */
static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,0.69314718055994530942);
}
/*
 * M_LN10
 *  Expand 	2.30258509299404568402
 */
static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,2.30258509299404568402);
}
/*
 * M_PI_2
 *  Expand 	1.57079632679489661923
 */
static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,1.57079632679489661923);
}
/*
 * M_PI_4
 *  Expand 	0.78539816339744830962
 */
static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,0.78539816339744830962);
}
/*
 * M_1_PI
 *  Expand 	0.31830988618379067154
 */
static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,0.31830988618379067154);
}
/*
 * M_2_PI
 *  Expand 0.63661977236758134308
 */
static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,0.63661977236758134308);
}
/*
 * M_SQRTPI
 *  Expand 1.77245385090551602729
 */
static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,1.77245385090551602729);
}
/*
 * M_2_SQRTPI
 *  Expand 	1.12837916709551257390
 */
static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,1.12837916709551257390);
}
/*
 * M_SQRT2
 *  Expand 	1.41421356237309504880
 */
static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,1.41421356237309504880);
}
/*
 * M_SQRT3
 *  Expand 	1.73205080756887729352
 */
static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,1.73205080756887729352);
}
/*
 * M_SQRT1_2
 *  Expand 	0.70710678118654752440
 */
static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,0.70710678118654752440);
}
/*
 * M_LNPI
 *  Expand 	1.14472988584940017414
 */
static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,1.14472988584940017414);
}
/*
 * M_EULER
 *  Expand  0.57721566490153286061
 */
static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,0.57721566490153286061);
}
#endif /* PH7_DISABLE_BUILTIN_MATH */
/*
 * SUNFUNCS_RET_TIMESTAMP / SUNFUNCS_RET_STRING / SUNFUNCS_RET_DOUBLE
 *  The three shapes date_sunrise() and date_sunset() can answer in: an
 *  absolute Unix timestamp, an "H:i" clock face, or the hour as a float.
 *  STRING is the default, which is why it is 1 rather than 0.
 */
static void PH7_SUNFUNCS_RET_TIMESTAMP_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0);
}
static void PH7_SUNFUNCS_RET_STRING_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
static void PH7_SUNFUNCS_RET_DOUBLE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,2);
}
/*
 * DATE_ATOM
 *  Expand Atom (example: 2005-08-15T15:52:01+00:00)
 */
static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);
}
/*
 * DATE_COOKIE
 *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)
 */
static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);
}
/*
 * DATE_ISO8601
 *  ISO-8601 (example: 2005-08-15T15:52:01+0000)
 */
static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);
}
/*
 * DATE_RFC822
 *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)
 */
static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);
}
/*
 * DATE_RFC850
 *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)
 */
static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);
}
/*
 * DATE_RFC1036
 *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)
 */
static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);
}
/*
 * DATE_RFC1123
 *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)
 */
static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);
}
/*
 * DATE_RFC2822
 *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)
 */
static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);
}
/*
 * DATE_RSS
 *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)
 */
static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);
}
/*
 * DATE_W3C
 *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)
 */
static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);
}
/*
 * The three format constants php added after the original set. Each is a plain
 * format STRING, so the whole of its behaviour is what date()/DateTime::format()
 * already do with those characters -- but each was a loud undefined-constant
 * fatal, which is a program that does not run rather than one that runs wrong.
 *
 * DATE_RFC7231 is the HTTP date (always GMT, so the zone letters are ESCAPED
 * rather than formatted -- php's own definition, and the reason it is not
 * DATE_RFC1123 with a T on the end). DATE_RFC3339_EXTENDED carries
 * milliseconds. DATE_ISO8601_EXPANDED uses `X`, the expanded-year field, where
 * the plain DATE_ISO8601 uses `Y`.
 */
static void PH7_DATE_RFC7231_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"D, d M Y H:i:s \\G\\M\\T",-1/*Compute length automatically*/);
}
static void PH7_DATE_RFC3339_EXTENDED_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"Y-m-d\\TH:i:s.vP",-1/*Compute length automatically*/);
}
static void PH7_DATE_ISO8601_EXPANDED_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"X-m-d\\TH:i:sP",-1/*Compute length automatically*/);
}
/*
 * FILE_TEXT / FILE_BINARY
 *  Both expand 0. php declares them for file()/file_put_contents()'s $flags and
 *  ignores them (the CLI has no text mode to select), but a program that names
 *  one still has to COMPILE, and an undefined constant is a fatal.
 */
static void PH7_FILE_TEXT_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,0);
	SXUNUSED(pUserData);
}
/*
 * The ENT_* values are PHP-exact (php 8.5.7). The low two bits are the quote
 * bits (1 = single, 2 = double), so ENT_QUOTES = ENT_COMPAT|1 and
 * ENT_NOQUOTES = 0. Bits 16|32 select the doctype (0 = HTML401, 16 = XML1,
 * 32 = XHTML, 48 = HTML5) — composites, not flags.
 */
/*
 * ENT_COMPAT
 *  Expand 2 (double-quote bit only)
 */
static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_ENT_QUOTE_DOUBLE);
}
/*
 * ENT_QUOTES
 *  Expand 3 (double|single quote bits)
 */
static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_ENT_QUOTES);
}
/*
 * ENT_NOQUOTES
 *  Expand 0 (no quote bits)
 */
static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0);
}
/*
 * ENT_IGNORE
 *  Expand 4
 */
static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_ENT_IGNORE);
}
/*
 * ENT_SUBSTITUTE
 *  Expand 8
 */
static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_ENT_SUBSTITUTE);
}
/*
 * ENT_DISALLOWED
 *  Expand 128
 */
static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_ENT_DISALLOWED);
}
/*
 * ENT_HTML401
 *  Expand 0 (the default doctype)
 */
static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_ENT_DOC_HTML401);
}
/*
 * ENT_XML1
 *  Expand 16
 */
static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_ENT_DOC_XML1);
}
/*
 * ENT_XHTML
 *  Expand 32
 */
static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_ENT_DOC_XHTML);
}
/*
 * ENT_HTML5
 *  Expand 48 (16|32 — a doctype composite, not a flag bit)
 */
static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_ENT_DOC_HTML5);
}
/*
 * ISO-8859-1
 * ISO_8859_1
 *   Expand 1
 */
static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
/*
 * UTF-8
 * UTF8
 *  Expand 2
 */
static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
/*
 * HTML_ENTITIES
 *  Expand 1
 */
static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
/*
 * HTML_SPECIALCHARS
 *  Expand 0 (PHP-exact)
 */
static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0);
}
/*
 * PHP_URL_SCHEME.
 * Expand 0
 */
static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0);
}
/*
 * PHP_URL_HOST.
 * Expand 1
 */
static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
/*
 * PHP_URL_PORT.
 * Expand 2
 */
static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,2);
}
/*
 * PHP_URL_USER.
 * Expand 3
 */
static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,3);
}
/*
 * PHP_URL_PASS.
 * Expand 4
 */
static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,4);
}
/*
 * PHP_URL_PATH.
 * Expand 5
 */
static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,5);
}
/*
 * PHP_URL_QUERY.
 * Expand 6
 */
static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,6);
}
/*
 * PHP_URL_FRAGMENT.
 * Expand 7
 */
static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,7);
}
/*
 * PHP_QUERY_RFC1738
 * Expand 1
 */
static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
/*
 * PHP_QUERY_RFC3986
 * Expand 1
 */
static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,2);
}
/* php's FNM_* values (ext/standard): PATHNAME=1, NOESCAPE=2, PERIOD=4, CASEFOLD=16.
 * PHL previously had PATHNAME/NOESCAPE swapped and CASEFOLD=8; fnmatch() reads these
 * bits, so PH7_builtin_fnmatch was updated to the same values. */
/*
 * FNM_PATHNAME
 *  Expand 1 (php value)
 */
static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
/*
 * FNM_NOESCAPE
 *  Expand 2 (php value)
 */
static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,2);
}
/*
 * FNM_PERIOD
 *  Expand 4 (php value)
 */
static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,4);
}
/*
 * FNM_CASEFOLD
 *  Expand 16 (php value)
 */
static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,16);
}
/*
 * PATHINFO_DIRNAME
 *  Expand 1.
 */
static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_PATHINFO_DIRNAME);
}
/*
 * PATHINFO_BASENAME
 *  Expand 2.
 */
static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_PATHINFO_BASENAME);
}
/*
 * PATHINFO_EXTENSION
 *  Expand php's 4 (a POWER OF TWO: the components are a bitmask).
 */
static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_PATHINFO_EXTENSION);
}
/*
 * PATHINFO_FILENAME
 *  Expand php's 8 (a POWER OF TWO: the components are a bitmask).
 */
static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_PATHINFO_FILENAME);
}
/*
 * PATHINFO_ALL
 *  Expand php's 15 — the default, and the one value that answers with the ARRAY.
 */
static void PH7_PATHINFO_ALL_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_PATHINFO_ALL);
}
#ifdef PH7_ENABLE_PCRE
/*
 * php's four PCRE build constants, asked of the linked library (see
 * PH7_PcreVersionInfo). Composer reads PCRE_VERSION before it loads a repository.
 */
static void PH7_PCRE_VERSION_Const(ph7_value *pVal,void *pUserData)
{
	char zVer[64];
	SXUNUSED(pUserData);
	PH7_PcreVersionInfo(zVer,(int)sizeof(zVer),0,0,0);
	ph7_value_string(pVal,zVer,-1);
}
static void PH7_PCRE_VERSION_MAJOR_Const(ph7_value *pVal,void *pUserData)
{
	char zVer[64];
	int iMaj = 0;
	SXUNUSED(pUserData);
	PH7_PcreVersionInfo(zVer,(int)sizeof(zVer),&iMaj,0,0);
	ph7_value_int(pVal,iMaj);
}
static void PH7_PCRE_VERSION_MINOR_Const(ph7_value *pVal,void *pUserData)
{
	char zVer[64];
	int iMin = 0;
	SXUNUSED(pUserData);
	PH7_PcreVersionInfo(zVer,(int)sizeof(zVer),0,&iMin,0);
	ph7_value_int(pVal,iMin);
}
static void PH7_PCRE_JIT_SUPPORT_Const(ph7_value *pVal,void *pUserData)
{
	char zVer[64];
	int iJit = 0;
	SXUNUSED(pUserData);
	PH7_PcreVersionInfo(zVer,(int)sizeof(zVer),0,0,&iJit);
	ph7_value_bool(pVal,iJit);
}
#endif /* PH7_ENABLE_PCRE */
/*
 * php's four BUILD-SHAPE booleans. A script reads them to decide what the engine
 * can do, not what it is called: symfony/process asks `defined('ZEND_THREAD_SAFE')`
 * to know whether `proc_open` needs an explicit cwd, and with the constant simply
 * ABSENT it passed null and every subprocess Composer runs died in `is_dir(null)`.
 * PHL runs one VM per thread with no shared globals, so it answers php's
 * non-ZTS, non-debug shape.
 */
static void PH7_ZEND_THREAD_SAFE_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_bool(pVal,0); }
static void PH7_ZEND_DEBUG_BUILD_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_bool(pVal,0); }
static void PH7_PHP_ZTS_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_bool(pVal,0); }
static void PH7_PHP_DEBUG_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_bool(pVal,0); }
/*
 * php's phpinfo() SECTION flags. A script passes one to say which part it wants;
 * symfony/process asks for INFO_GENERAL to read the build's configure line (it is
 * how it detects `--enable-sigchild`), so Composer needs them to start at all.
 */
static void PH7_INFO_GENERAL_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int(pVal,1); }
static void PH7_INFO_CREDITS_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int(pVal,2); }
static void PH7_INFO_CONFIGURATION_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int(pVal,4); }
static void PH7_INFO_MODULES_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int(pVal,8); }
static void PH7_INFO_ENVIRONMENT_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int(pVal,16); }
static void PH7_INFO_VARIABLES_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int(pVal,32); }
static void PH7_INFO_LICENSE_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int(pVal,64); }
static void PH7_INFO_ALL_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,4294967295LL); }
/*
 * The LC_* CATEGORY numbers. php registers the C library's own macros, so its
 * numbers are the platform's (macOS and Windows put LC_ALL at 0); these are
 * glibc's on every platform -- a script that uses the names cannot tell, one
 * that prints the numbers can -- and setlocale() maps them to the platform's
 * macros.
 */
static void PH7_LC_CTYPE_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int(pVal,0); }
static void PH7_LC_NUMERIC_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int(pVal,1); }
static void PH7_LC_TIME_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int(pVal,2); }
static void PH7_LC_COLLATE_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int(pVal,3); }
static void PH7_LC_MONETARY_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int(pVal,4); }
static void PH7_LC_MESSAGES_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int(pVal,5); }
static void PH7_LC_ALL_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int(pVal,6); }
/*
 * ext/posix's constants. Every one of them is the PLATFORM's macro rather than
 * a number copied out of one build: `RLIMIT_AS` is 9 on Linux and something
 * else elsewhere, and a script that stores one and hands it back to
 * posix_getrlimit() has to get its own system's answer. php builds no
 * ext/posix on Windows, so none of these is defined there either.
 */
#ifndef __WINNT__
#ifdef F_OK
static void PH7_POSIX_F_OK_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)F_OK); }
#endif
#ifdef X_OK
static void PH7_POSIX_X_OK_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)X_OK); }
#endif
#ifdef W_OK
static void PH7_POSIX_W_OK_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)W_OK); }
#endif
#ifdef R_OK
static void PH7_POSIX_R_OK_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)R_OK); }
#endif
#ifdef S_IFREG
static void PH7_POSIX_S_IFREG_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFREG); }
#endif
#ifdef S_IFCHR
static void PH7_POSIX_S_IFCHR_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFCHR); }
#endif
#ifdef S_IFBLK
static void PH7_POSIX_S_IFBLK_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFBLK); }
#endif
#ifdef S_IFIFO
static void PH7_POSIX_S_IFIFO_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFIFO); }
#endif
#ifdef S_IFSOCK
static void PH7_POSIX_S_IFSOCK_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFSOCK); }
#endif
#ifdef RLIMIT_AS
static void PH7_POSIX_RLIMIT_AS_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_AS); }
#endif
#ifdef RLIMIT_CORE
static void PH7_POSIX_RLIMIT_CORE_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_CORE); }
#endif
#ifdef RLIMIT_CPU
static void PH7_POSIX_RLIMIT_CPU_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_CPU); }
#endif
#ifdef RLIMIT_DATA
static void PH7_POSIX_RLIMIT_DATA_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_DATA); }
#endif
#ifdef RLIMIT_FSIZE
static void PH7_POSIX_RLIMIT_FSIZE_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_FSIZE); }
#endif
#ifdef RLIMIT_LOCKS
static void PH7_POSIX_RLIMIT_LOCKS_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_LOCKS); }
#endif
#ifdef RLIMIT_MEMLOCK
static void PH7_POSIX_RLIMIT_MEMLOCK_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_MEMLOCK); }
#endif
#ifdef RLIMIT_MSGQUEUE
static void PH7_POSIX_RLIMIT_MSGQUEUE_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_MSGQUEUE); }
#endif
#ifdef RLIMIT_NICE
static void PH7_POSIX_RLIMIT_NICE_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_NICE); }
#endif
#ifdef RLIMIT_NOFILE
static void PH7_POSIX_RLIMIT_NOFILE_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_NOFILE); }
#endif
#ifdef RLIMIT_NPROC
static void PH7_POSIX_RLIMIT_NPROC_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_NPROC); }
#endif
#ifdef RLIMIT_RSS
static void PH7_POSIX_RLIMIT_RSS_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_RSS); }
#endif
#ifdef RLIMIT_RTPRIO
static void PH7_POSIX_RLIMIT_RTPRIO_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_RTPRIO); }
#endif
#ifdef RLIMIT_RTTIME
static void PH7_POSIX_RLIMIT_RTTIME_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_RTTIME); }
#endif
#ifdef RLIMIT_SIGPENDING
static void PH7_POSIX_RLIMIT_SIGPENDING_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_SIGPENDING); }
#endif
#ifdef RLIMIT_STACK
static void PH7_POSIX_RLIMIT_STACK_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_STACK); }
#endif
#ifdef _SC_ARG_MAX
static void PH7_POSIX_SC_ARG_MAX_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_ARG_MAX); }
#endif
#ifdef _SC_CHILD_MAX
static void PH7_POSIX_SC_CHILD_MAX_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_CHILD_MAX); }
#endif
#ifdef _SC_CLK_TCK
static void PH7_POSIX_SC_CLK_TCK_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_CLK_TCK); }
#endif
#ifdef _SC_OPEN_MAX
static void PH7_POSIX_SC_OPEN_MAX_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_OPEN_MAX); }
#endif
#ifdef _SC_PAGESIZE
static void PH7_POSIX_SC_PAGESIZE_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_PAGESIZE); }
#endif
#ifdef _SC_NPROCESSORS_CONF
static void PH7_POSIX_SC_NPROCESSORS_CONF_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_NPROCESSORS_CONF); }
#endif
#ifdef _SC_NPROCESSORS_ONLN
static void PH7_POSIX_SC_NPROCESSORS_ONLN_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_NPROCESSORS_ONLN); }
#endif
#ifdef _PC_LINK_MAX
static void PH7_POSIX_PC_LINK_MAX_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_LINK_MAX); }
#endif
#ifdef _PC_MAX_CANON
static void PH7_POSIX_PC_MAX_CANON_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_MAX_CANON); }
#endif
#ifdef _PC_MAX_INPUT
static void PH7_POSIX_PC_MAX_INPUT_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_MAX_INPUT); }
#endif
#ifdef _PC_NAME_MAX
static void PH7_POSIX_PC_NAME_MAX_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_NAME_MAX); }
#endif
#ifdef _PC_PATH_MAX
static void PH7_POSIX_PC_PATH_MAX_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_PATH_MAX); }
#endif
#ifdef _PC_PIPE_BUF
static void PH7_POSIX_PC_PIPE_BUF_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_PIPE_BUF); }
#endif
#ifdef _PC_CHOWN_RESTRICTED
static void PH7_POSIX_PC_CHOWN_RESTRICTED_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_CHOWN_RESTRICTED); }
#endif
#ifdef _PC_NO_TRUNC
static void PH7_POSIX_PC_NO_TRUNC_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_NO_TRUNC); }
#endif
#ifdef _PC_ALLOC_SIZE_MIN
static void PH7_POSIX_PC_ALLOC_SIZE_MIN_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_ALLOC_SIZE_MIN); }
#endif
#ifdef _PC_SYMLINK_MAX
static void PH7_POSIX_PC_SYMLINK_MAX_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_SYMLINK_MAX); }
#endif
/*
 * RLIM_INFINITY is a rlim_t, which is unsigned; php answers it as -1, which is
 * what posix_setrlimit() takes back for "no limit".
 */
static void PH7_POSIX_RLIMIT_INFINITY_Const(ph7_value *pVal,void *pUserData)
{ SXUNUSED(pUserData); ph7_value_int64(pVal,-1); }
#endif /* __WINNT__ */
/*
 * SEEK_SET.
 *  Expand 0
 */
static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0);
}
/*
 * SEEK_CUR.
 *  Expand 1
 */
static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
/*
 * SEEK_END.
 *  Expand 2
 */
static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,2);
}
/*
 * LOCK_SH.
 *  Expand 2
 */
static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
/*
 * LOCK_NB.
 *  Expand 4 (php)
 */
static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,4);
}
/*
 * LOCK_EX.
 *  Expand 2 (php). PH7 used 1, which collided with LOCK_SH, and LOCK_UN was 0 — so
 *  flock($h, LOCK_UN) asked the stream for a SHARED lock instead of releasing one.
 */
static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,2);
}
/*
 * LOCK_UN.
 *  Expand 3 (php)
 */
static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,3);
}
/*
 * FILE_USE_INCLUDE_PATH
 *  Expand 0x01 (Must be a power of two)
 */
static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x1);
}
/*
 * FILE_IGNORE_NEW_LINES
 *  Expand 0x02 (Must be a power of two)
 */
static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x2);
}
/*
 * FILE_SKIP_EMPTY_LINES
 *  Expand 0x04 (Must be a power of two)
 */
static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x4);
}
/*
 * FILE_APPEND
 *  Expand 0x08 (Must be a power of two)
 */
static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x08);
}
/*
 * FILE_NO_DEFAULT_CONTEXT
 *  Expand php's 0x10. file()/file_put_contents() read it: it is what stops the
 *  `$context = null` argument from resolving to stream_context_get_default()'s
 *  context. What a device then does with an open carrying no context is its own
 *  business — a userland wrapper's $this->context is a resource either way, in
 *  php as here — so the flag is only observable where an option is consumed.
 */
static void PH7_FILE_NO_DEFAULT_CONTEXT_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x10);
}
/*
 * SCANDIR_SORT_ASCENDING
 *  Expand 0
 */
static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0);
}
/*
 * SCANDIR_SORT_DESCENDING
 *  Expand 1
 */
static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
/*
 * SCANDIR_SORT_NONE
 *  Expand 2
 */
static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,2);
}
/*
 * GLOB_MARK
 *  Expand php's 0x08 (php's own portable glob flag set)
 */
static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_GLOB_MARK);
}
/*
 * GLOB_NOSORT
 *  Expand php's 0x20
 */
static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_GLOB_NOSORT);
}
/*
 * GLOB_NOCHECK
 *  Expand php's 0x10
 */
static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_GLOB_NOCHECK);
}
/*
 * GLOB_NOESCAPE
 *  Expand php's 0x1000
 */
static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_GLOB_NOESCAPE);
}
/*
 * GLOB_BRACE
 *  Expand php's 0x80
 */
static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_GLOB_BRACE);
}
/*
 * GLOB_ONLYDIR
 *  Expand php's 0x40000000
 */
static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_GLOB_ONLYDIR);
}
/*
 * GLOB_ERR
 *  Expand php's 0x04
 */
static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_GLOB_ERR);
}
/*
 * GLOB_AVAILABLE_FLAGS
 *  Expand the OR of every glob flag php's portable glob accepts — the mask
 *  glob() itself validates against (1073746108 on every platform, since the
 *  GLOB_* values are php 8.5's own portable set).
 */
static void PH7_GLOB_AVAILABLE_FLAGS_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_GLOB_ERR|PH7_GLOB_MARK|PH7_GLOB_NOCHECK|PH7_GLOB_NOSORT
		|PH7_GLOB_BRACE|PH7_GLOB_NOESCAPE|PH7_GLOB_ONLYDIR);
}
/*
 * STDIN
 *  Expand the STDIN handle as a resource.
 */
static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)
{
	ph7_vm *pVm = (ph7_vm *)pUserData;
	void *pResource;
	pResource = PH7_ExportStdin(pVm);
	ph7_value_resource(pVal,pResource);
}
/*
 * STDOUT
 *   Expand the STDOUT handle as a resource.
 */
static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)
{
	ph7_vm *pVm = (ph7_vm *)pUserData;
	void *pResource;
	pResource = PH7_ExportStdout(pVm);
	ph7_value_resource(pVal,pResource);
}
/*
 * STDERR
 *  Expand the STDERR handle as a resource.
 */
static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)
{
	ph7_vm *pVm = (ph7_vm *)pUserData;
	void *pResource;
	pResource = PH7_ExportStderr(pVm);
	ph7_value_resource(pVal,pResource);
}
/*
 * INI_SCANNER_NORMAL
 *   Expand php's 0
 */
static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_INI_SCANNER_NORMAL);
}
/*
 * INI_SCANNER_RAW
 *   Expand php's 1
 */
static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_INI_SCANNER_RAW);
}
/*
 * INI_SCANNER_TYPED
 *   Expand 2 (php's value)
 */
static void PH7_INI_SCANNER_TYPED_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_INI_SCANNER_TYPED);
}
/*
 * EXTR_OVERWRITE
 *   Expand 0 (php's enum value; see PH7_EXTR_* in ph7int.h)
 */
static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_EXTR_OVERWRITE);
}
/*
 * EXTR_SKIP
 *   Expand 1
 */
static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_EXTR_SKIP);
}
/*
 * EXTR_PREFIX_SAME
 *   Expand 2
 */
static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_EXTR_PREFIX_SAME);
}
/*
 * EXTR_PREFIX_ALL
 *   Expand 3
 */
static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_EXTR_PREFIX_ALL);
}
/*
 * EXTR_PREFIX_INVALID
 *   Expand 4
 */
static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_EXTR_PREFIX_INVALID);
}
/*
 * EXTR_IF_EXISTS
 *   Expand 6 (php orders IF_EXISTS after PREFIX_IF_EXISTS)
 */
static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_EXTR_IF_EXISTS);
}
/*
 * EXTR_REFS
 *   Expand 256 (the bit that rides above the mode: bind by REFERENCE)
 */
static void PH7_EXTR_REFS_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_EXTR_REFS);
}
/*
 * EXTR_PREFIX_IF_EXISTS
 *   Expand 5
 */
static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_EXTR_PREFIX_IF_EXISTS);
}
#ifndef PH7_DISABLE_HASH_FUNC
/*
 * HASH_HMAC.
 *   php's one hash_init() flag. Declared with the hash extension it belongs
 *   to, so a build without that extension has no constant either.
 */
static void PH7_HASH_HMAC_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_HASH_HMAC);
}
#endif /* PH7_DISABLE_HASH_FUNC */
/*
 * ICONV_* — what the converter IS, and iconv_mime_decode()'s $mode bits.
 * php reports the C library behind its extension here (`glibc`, `libiconv`);
 * PHL converts with its own code so that a Windows build answers what a POSIX
 * one does, and says so — the constants exist to be READ, and a program that
 * branches on them has to see something true.
 */
static void PH7_ICONV_IMPL_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_string(pVal,"PHL",(int)sizeof("PHL")-1);
}
static void PH7_ICONV_VERSION_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_string(pVal,PH7_VERSION,(int)sizeof(PH7_VERSION)-1);
}
static void PH7_ICONV_MIME_DECODE_STRICT_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,1);
}
static void PH7_ICONV_MIME_DECODE_CONTINUE_ON_ERROR_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,2);
}
/*
 * JSON_HEX_TAG.
 *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.
 */
static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_HEX_TAG);
}
/*
 * JSON_HEX_AMP.
 *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.
 */
static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_HEX_AMP);
}
/*
 * JSON_HEX_APOS.
 *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.
 */
static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_HEX_APOS);
}
/*
 * JSON_HEX_QUOT.
 *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.
 */
static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_HEX_QUOT);
}
/*
 * JSON_FORCE_OBJECT.
 *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.
 */
static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_FORCE_OBJECT);
}
/*
 * JSON_NUMERIC_CHECK.
 *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.
 */
static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_NUMERIC_CHECK);
}
/*
 * JSON_BIGINT_AS_STRING.
 *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.
 */
static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);
}
/*
 * JSON_PARTIAL_OUTPUT_ON_ERROR.
 *   Expand the value of JSON_PARTIAL_OUTPUT_ON_ERROR defined in ph7Int.h.
 */
static void PH7_JSON_PARTIAL_OUTPUT_ON_ERROR_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_PARTIAL_OUTPUT_ON_ERROR);
}
/*
 * JSON_PRESERVE_ZERO_FRACTION.
 *   Expand the value of JSON_PRESERVE_ZERO_FRACTION defined in ph7Int.h.
 */
static void PH7_JSON_PRESERVE_ZERO_FRACTION_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_PRESERVE_ZERO_FRACTION);
}
/*
 * JSON_OBJECT_AS_ARRAY.
 *   Expand the value of JSON_OBJECT_AS_ARRAY defined in ph7Int.h.
 */
static void PH7_JSON_OBJECT_AS_ARRAY_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_OBJECT_AS_ARRAY);
}
/*
 * JSON_PRETTY_PRINT.
 *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.
 */
static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_PRETTY_PRINT);
}
/*
 * JSON_UNESCAPED_SLASHES.
 *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.
 */
static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);
}
/*
 * JSON_UNESCAPED_UNICODE.
 *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.
 */
static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);
}
/*
 * JSON_UNESCAPED_LINE_TERMINATORS.
 *   Expand the value of JSON_UNESCAPED_LINE_TERMINATORS defined in ph7Int.h.
 */
static void PH7_JSON_UNESCAPED_LINE_TERMINATORS_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_UNESCAPED_LINE_TERMINATORS);
}
/*
 * JSON_INVALID_UTF8_IGNORE.
 *   Expand the value of JSON_INVALID_UTF8_IGNORE defined in ph7Int.h.
 */
static void PH7_JSON_INVALID_UTF8_IGNORE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_INVALID_UTF8_IGNORE);
}
/*
 * JSON_INVALID_UTF8_SUBSTITUTE.
 *   Expand the value of JSON_INVALID_UTF8_SUBSTITUTE defined in ph7Int.h.
 */
static void PH7_JSON_INVALID_UTF8_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_INVALID_UTF8_SUBSTITUTE);
}
/*
 * JSON_THROW_ON_ERROR.
 *   Expand the value of JSON_THROW_ON_ERROR defined in ph7Int.h.
 */
static void PH7_JSON_THROW_ON_ERROR_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_THROW_ON_ERROR);
}
/*
 * JSON_ERROR_NONE.
 *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.
 */
static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_ERROR_NONE);
}
/*
 * JSON_ERROR_DEPTH.
 *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.
 */
static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_ERROR_DEPTH);
}
/*
 * JSON_ERROR_STATE_MISMATCH.
 *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.
 */
static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);
}
/*
 * JSON_ERROR_CTRL_CHAR.
 *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.
 */
static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);
}
/*
 * JSON_ERROR_SYNTAX.
 *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.
 */
static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_ERROR_SYNTAX);
}
/*
 * JSON_ERROR_UTF8.
 *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.
 */
static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_ERROR_UTF8);
}
/*
 * JSON_ERROR_RECURSION.
 *   Expand the value of JSON_ERROR_RECURSION defined in ph7Int.h.
 */
static void PH7_JSON_ERROR_RECURSION_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_ERROR_RECURSION);
}
/*
 * JSON_ERROR_UNSUPPORTED_TYPE.
 *   Expand the value of JSON_ERROR_UNSUPPORTED_TYPE defined in ph7Int.h.
 */
static void PH7_JSON_ERROR_UNSUPPORTED_TYPE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_ERROR_UNSUPPORTED_TYPE);
}
/*
 * JSON_ERROR_INVALID_PROPERTY_NAME.
 *   Expand the value of JSON_ERROR_INVALID_PROPERTY_NAME defined in ph7Int.h.
 */
static void PH7_JSON_ERROR_INVALID_PROPERTY_NAME_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_ERROR_INVALID_PROPERTY_NAME);
}
/*
 * JSON_ERROR_UTF16.
 *   Expand the value of JSON_ERROR_UTF16 defined in ph7Int.h.
 */
static void PH7_JSON_ERROR_UTF16_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_ERROR_UTF16);
}
/*
 * JSON_ERROR_NON_BACKED_ENUM.
 *   Expand the value of JSON_ERROR_NON_BACKED_ENUM defined in ph7Int.h (php 8.1).
 */
static void PH7_JSON_ERROR_NON_BACKED_ENUM_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_ERROR_NON_BACKED_ENUM);
}
/*
 * JSON_ERROR_INF_OR_NAN.
 *   Expand the value of JSON_ERROR_INF_OR_NAN defined in ph7Int.h.
 */
static void PH7_JSON_ERROR_INF_OR_NAN_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_ERROR_INF_OR_NAN);
}
/*
 * __CLASS__
 *  The current class name, or the EMPTY STRING outside any class — php answers "",
 *  not null (`__CLASS__ === ""` is true in global scope). `self` keeps its own
 *  expander below because php treats IT differently outside a class scope.
 */
static void PH7_class_magic_Const(ph7_value *pVal,void *pUserData)
{
	ph7_vm *pVm = (ph7_vm *)pUserData;
	ph7_class *pClass;
	/* php flattens a trait into the class that used it, so __CLASS__ inside a trait method
	 * is THAT class (where __TRAIT__ and __METHOD__ stay the trait's — php's own asymmetry). */
	pClass = PH7_VmPeekSelfClass(pVm);
	if( pClass == 0 ){
		pClass = PH7_VmPeekTopClass(pVm);
	}
	if( pClass ){
		SyString *pName = &pClass->sName;
		ph7_value_string(pVal,pName->zString,(int)pName->nByte);
	}else{
		ph7_value_string(pVal,"",0);
	}
}

/*
 * PASSWORD_BCRYPT / PASSWORD_DEFAULT
 *  The bcrypt algorithm identifier (PHP 7.4+ exposes these as the string "2y").
 *  PASSWORD_DEFAULT tracks the recommended default, currently bcrypt.
 */
static void PH7_PASSWORD_BCRYPT_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_string(pVal,"2y",(int)sizeof("2y")-1);
}
/*
 * PASSWORD_BCRYPT_DEFAULT_COST
 *  The default bcrypt work factor used by password_hash() (currently 12).
 */
static void PH7_PASSWORD_COST_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,12);
}
/*
 * PASSWORD_ARGON2I / PASSWORD_ARGON2ID and the three argon2 option defaults
 * password_hash() reads when $options omits them.
 */
static void PH7_PASSWORD_ARGON2I_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_string(pVal,"argon2i",(int)sizeof("argon2i")-1);
}
static void PH7_PASSWORD_ARGON2ID_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_string(pVal,"argon2id",(int)sizeof("argon2id")-1);
}
static void PH7_ARGON2_MEM_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,65536);
}
static void PH7_ARGON2_TIME_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,4);
}
static void PH7_ARGON2_THREADS_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,1);
}
/*
 * CRYPT_* — the crypt() capability flags. Every scheme is compiled in, so all
 * six are 1, and CRYPT_SALT_LENGTH is php's 123 (the longest setting string a
 * SHA-512-crypt with an explicit rounds count can need).
 */
#ifndef PH7_DISABLE_HASH_FUNC
static void PH7_CRYPT_ONE_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,1);
}
static void PH7_CRYPT_SALT_LENGTH_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int(pVal,123);
}
#endif /* PH7_DISABLE_HASH_FUNC */
/*
 * filter_var() filter and flag identifiers (the ext/filter constants). Values
 * match PHP 8.5. One tiny int-returning callback per constant, generated by a
 * local macro to keep the ~25 near-identical definitions DRY.
 */
#define PH7_FILTER_INT_CONST(Name,Val) \
	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \
		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \
	}
PH7_FILTER_INT_CONST(FILTER_DEFAULT,516)
PH7_FILTER_INT_CONST(FILTER_UNSAFE_RAW,516)
PH7_FILTER_INT_CONST(FILTER_VALIDATE_INT,257)
PH7_FILTER_INT_CONST(FILTER_VALIDATE_BOOLEAN,258)
PH7_FILTER_INT_CONST(FILTER_VALIDATE_FLOAT,259)
PH7_FILTER_INT_CONST(FILTER_VALIDATE_REGEXP,272)
PH7_FILTER_INT_CONST(FILTER_VALIDATE_DOMAIN,277)
PH7_FILTER_INT_CONST(FILTER_VALIDATE_URL,273)
PH7_FILTER_INT_CONST(FILTER_VALIDATE_EMAIL,274)
PH7_FILTER_INT_CONST(FILTER_VALIDATE_IP,275)
PH7_FILTER_INT_CONST(FILTER_VALIDATE_MAC,276)
PH7_FILTER_INT_CONST(FILTER_CALLBACK,1024)
PH7_FILTER_INT_CONST(FILTER_THROW_ON_FAILURE,268435456)
PH7_FILTER_INT_CONST(FILTER_SANITIZE_ENCODED,514)
PH7_FILTER_INT_CONST(FILTER_SANITIZE_ADD_SLASHES,523)
PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_INT,519)
PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_FLOAT,520)
PH7_FILTER_INT_CONST(FILTER_SANITIZE_SPECIAL_CHARS,515)
PH7_FILTER_INT_CONST(FILTER_SANITIZE_FULL_SPECIAL_CHARS,522)
PH7_FILTER_INT_CONST(FILTER_SANITIZE_EMAIL,517)
PH7_FILTER_INT_CONST(FILTER_SANITIZE_URL,518)
PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_OCTAL,1)
PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_HEX,2)
PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_LOW,4)
PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_HIGH,8)
PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_LOW,16)
PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_HIGH,32)
PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_AMP,64)
PH7_FILTER_INT_CONST(FILTER_FLAG_NO_ENCODE_QUOTES,128)
PH7_FILTER_INT_CONST(FILTER_FLAG_NONE,0)
PH7_FILTER_INT_CONST(FILTER_FLAG_EMPTY_STRING_NULL,256)
PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_BACKTICK,512)
PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_FRACTION,4096)
PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_THOUSAND,8192)
PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_SCIENTIFIC,16384)
PH7_FILTER_INT_CONST(FILTER_FLAG_IPV4,1048576)
PH7_FILTER_INT_CONST(FILTER_FLAG_IPV6,2097152)
PH7_FILTER_INT_CONST(FILTER_FLAG_PATH_REQUIRED,262144)
PH7_FILTER_INT_CONST(FILTER_FLAG_QUERY_REQUIRED,524288)
PH7_FILTER_INT_CONST(FILTER_FLAG_HOSTNAME,1048576)
PH7_FILTER_INT_CONST(FILTER_FLAG_EMAIL_UNICODE,1048576)
PH7_FILTER_INT_CONST(FILTER_FLAG_NO_RES_RANGE,4194304)
PH7_FILTER_INT_CONST(FILTER_FLAG_NO_PRIV_RANGE,8388608)
PH7_FILTER_INT_CONST(FILTER_FLAG_GLOBAL_RANGE,536870912)
PH7_FILTER_INT_CONST(FILTER_REQUIRE_ARRAY,16777216)
PH7_FILTER_INT_CONST(FILTER_REQUIRE_SCALAR,33554432)
PH7_FILTER_INT_CONST(FILTER_FORCE_ARRAY,67108864)
PH7_FILTER_INT_CONST(FILTER_NULL_ON_FAILURE,134217728)
/* filter_input() source selectors (php values; SESSION/REQUEST are undefined in 8.5) */
PH7_FILTER_INT_CONST(INPUT_POST,0)
PH7_FILTER_INT_CONST(INPUT_GET,1)
PH7_FILTER_INT_CONST(INPUT_COOKIE,2)
PH7_FILTER_INT_CONST(INPUT_ENV,4)
PH7_FILTER_INT_CONST(INPUT_SERVER,5)
/*
 * ext/fileinfo's flags. libmagic's MAGIC_* values, which php re-exports under
 * its own names -- a script stores one and hands it back, so the NUMBERS are
 * the contract (see builtin_fileinfo.c).
 */
#define PH7_FILEINFO_INT_CONST(Name,Val) \
	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \
		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \
	}
/*
 * ext/zlib's constants. The three encodings are libz's own windowBits spelling
 * (negative = raw, +16 = gzip), which is why they are -15/15/31; the flush and
 * status numbers are libz's too. ZLIB_VERSION and ZLIB_VERNUM report the
 * library this build was compiled against, exactly as php's report theirs --
 * so they are the one pair here that is not the same on every box.
 */
#ifdef PH7_ENABLE_ZLIB
#define PH7_ZLIB_INT_CONST(Name,Val) \
	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \
		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \
	}
PH7_ZLIB_INT_CONST(FORCE_GZIP,31)
PH7_ZLIB_INT_CONST(FORCE_DEFLATE,15)
PH7_ZLIB_INT_CONST(ZLIB_ENCODING_RAW,-15)
PH7_ZLIB_INT_CONST(ZLIB_ENCODING_GZIP,31)
PH7_ZLIB_INT_CONST(ZLIB_ENCODING_DEFLATE,15)
PH7_ZLIB_INT_CONST(ZLIB_NO_FLUSH,0)
PH7_ZLIB_INT_CONST(ZLIB_PARTIAL_FLUSH,1)
PH7_ZLIB_INT_CONST(ZLIB_SYNC_FLUSH,2)
PH7_ZLIB_INT_CONST(ZLIB_FULL_FLUSH,3)
PH7_ZLIB_INT_CONST(ZLIB_BLOCK,5)
PH7_ZLIB_INT_CONST(ZLIB_FINISH,4)
PH7_ZLIB_INT_CONST(ZLIB_FILTERED,1)
PH7_ZLIB_INT_CONST(ZLIB_HUFFMAN_ONLY,2)
PH7_ZLIB_INT_CONST(ZLIB_RLE,3)
PH7_ZLIB_INT_CONST(ZLIB_FIXED,4)
PH7_ZLIB_INT_CONST(ZLIB_DEFAULT_STRATEGY,0)
PH7_ZLIB_INT_CONST(ZLIB_VERNUM,ZLIB_VERNUM)
PH7_ZLIB_INT_CONST(ZLIB_OK,0)
PH7_ZLIB_INT_CONST(ZLIB_STREAM_END,1)
PH7_ZLIB_INT_CONST(ZLIB_NEED_DICT,2)
PH7_ZLIB_INT_CONST(ZLIB_ERRNO,-1)
PH7_ZLIB_INT_CONST(ZLIB_STREAM_ERROR,-2)
PH7_ZLIB_INT_CONST(ZLIB_DATA_ERROR,-3)
PH7_ZLIB_INT_CONST(ZLIB_MEM_ERROR,-4)
PH7_ZLIB_INT_CONST(ZLIB_BUF_ERROR,-5)
PH7_ZLIB_INT_CONST(ZLIB_VERSION_ERROR,-6)
static void PH7_ZLIB_VERSION_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_string(pVal,ZLIB_VERSION,-1);
}
#endif /* PH7_ENABLE_ZLIB */
/*
 * ext/openssl's constants. THREE families with three different origins, and
 * telling them apart is the whole of the work here:
 *
 *  - the LIBRARY's numbers (the X509_PURPOSE_*, the PKCS7_* and CMS_* flag
 *    bits, the RSA padding modes, the version pair). Each is bound to the
 *    OpenSSL SYMBOL, never to the number this box happens to print: a flag
 *    that moved between 3.0 and 3.6 would otherwise be silently wrong on the
 *    Windows build, which links a different one. See the
 *    `bind-by-symbol-not-by-value` note.
 *  - php's OWN numbering, which OpenSSL has no symbol for: OPENSSL_ALGO_*
 *    (a digest enum of php's, with the gap at 4/5 where php removed two DSS
 *    entries), OPENSSL_CIPHER_* (an enum the PKCS#7 doors take),
 *    OPENSSL_KEYTYPE_*, the three option bits and the three CMS encodings.
 *    These are literals because they ARE literals -- a script stores one and
 *    hands it back, so the numbers are the contract.
 *  - one STRING php composes itself: OPENSSL_DEFAULT_STREAM_CIPHERS, the
 *    cipher list php's own TLS streams start from. It is php's text, not
 *    OpenSSL's default, and it is byte-for-byte what php ships.
 */
#ifdef PH7_ENABLE_OPENSSL
#define PH7_SSL_INT_CONST(Name,Val) \
	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \
		SXUNUSED(pUnused); ph7_value_int64(pVal,(sxi64)(Val)); \
	}
PH7_SSL_INT_CONST(OPENSSL_VERSION_NUMBER,OPENSSL_VERSION_NUMBER)
PH7_SSL_INT_CONST(X509_PURPOSE_SSL_CLIENT,X509_PURPOSE_SSL_CLIENT)
PH7_SSL_INT_CONST(X509_PURPOSE_SSL_SERVER,X509_PURPOSE_SSL_SERVER)
PH7_SSL_INT_CONST(X509_PURPOSE_NS_SSL_SERVER,X509_PURPOSE_NS_SSL_SERVER)
PH7_SSL_INT_CONST(X509_PURPOSE_SMIME_SIGN,X509_PURPOSE_SMIME_SIGN)
PH7_SSL_INT_CONST(X509_PURPOSE_SMIME_ENCRYPT,X509_PURPOSE_SMIME_ENCRYPT)
PH7_SSL_INT_CONST(X509_PURPOSE_CRL_SIGN,X509_PURPOSE_CRL_SIGN)
PH7_SSL_INT_CONST(X509_PURPOSE_ANY,X509_PURPOSE_ANY)
PH7_SSL_INT_CONST(X509_PURPOSE_OCSP_HELPER,X509_PURPOSE_OCSP_HELPER)
PH7_SSL_INT_CONST(X509_PURPOSE_TIMESTAMP_SIGN,X509_PURPOSE_TIMESTAMP_SIGN)
PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA1,1)
PH7_SSL_INT_CONST(OPENSSL_ALGO_MD5,2)
PH7_SSL_INT_CONST(OPENSSL_ALGO_MD4,3)
PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA224,6)
PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA256,7)
PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA384,8)
PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA512,9)
PH7_SSL_INT_CONST(OPENSSL_ALGO_RMD160,10)
PH7_SSL_INT_CONST(PKCS7_DETACHED,PKCS7_DETACHED)
PH7_SSL_INT_CONST(PKCS7_TEXT,PKCS7_TEXT)
PH7_SSL_INT_CONST(PKCS7_NOINTERN,PKCS7_NOINTERN)
PH7_SSL_INT_CONST(PKCS7_NOVERIFY,PKCS7_NOVERIFY)
PH7_SSL_INT_CONST(PKCS7_NOCHAIN,PKCS7_NOCHAIN)
PH7_SSL_INT_CONST(PKCS7_NOCERTS,PKCS7_NOCERTS)
PH7_SSL_INT_CONST(PKCS7_NOATTR,PKCS7_NOATTR)
PH7_SSL_INT_CONST(PKCS7_BINARY,PKCS7_BINARY)
PH7_SSL_INT_CONST(PKCS7_NOSIGS,PKCS7_NOSIGS)
PH7_SSL_INT_CONST(PKCS7_NOOLDMIMETYPE,PKCS7_NOOLDMIMETYPE)
PH7_SSL_INT_CONST(PKCS7_NOSMIMECAP,PKCS7_NOSMIMECAP)
PH7_SSL_INT_CONST(PKCS7_CRLFEOL,PKCS7_CRLFEOL)
PH7_SSL_INT_CONST(PKCS7_NOCRL,PKCS7_NOCRL)
PH7_SSL_INT_CONST(PKCS7_NO_DUAL_CONTENT,PKCS7_NO_DUAL_CONTENT)
PH7_SSL_INT_CONST(OPENSSL_CMS_DETACHED,CMS_DETACHED)
PH7_SSL_INT_CONST(OPENSSL_CMS_TEXT,CMS_TEXT)
PH7_SSL_INT_CONST(OPENSSL_CMS_NOINTERN,CMS_NOINTERN)
PH7_SSL_INT_CONST(OPENSSL_CMS_NOVERIFY,CMS_NO_SIGNER_CERT_VERIFY)
PH7_SSL_INT_CONST(OPENSSL_CMS_NOCERTS,CMS_NOCERTS)
PH7_SSL_INT_CONST(OPENSSL_CMS_NOATTR,CMS_NOATTR)
PH7_SSL_INT_CONST(OPENSSL_CMS_BINARY,CMS_BINARY)
PH7_SSL_INT_CONST(OPENSSL_CMS_NOSIGS,CMS_NOSIGS)
PH7_SSL_INT_CONST(OPENSSL_CMS_OLDMIMETYPE,CMS_NOOLDMIMETYPE)
PH7_SSL_INT_CONST(OPENSSL_PKCS1_PADDING,RSA_PKCS1_PADDING)
PH7_SSL_INT_CONST(OPENSSL_NO_PADDING,RSA_NO_PADDING)
PH7_SSL_INT_CONST(OPENSSL_PKCS1_OAEP_PADDING,RSA_PKCS1_OAEP_PADDING)
PH7_SSL_INT_CONST(OPENSSL_PKCS1_PSS_PADDING,RSA_PKCS1_PSS_PADDING)
PH7_SSL_INT_CONST(OPENSSL_CIPHER_RC2_40,0)
PH7_SSL_INT_CONST(OPENSSL_CIPHER_RC2_128,1)
PH7_SSL_INT_CONST(OPENSSL_CIPHER_RC2_64,2)
PH7_SSL_INT_CONST(OPENSSL_CIPHER_DES,3)
PH7_SSL_INT_CONST(OPENSSL_CIPHER_3DES,4)
PH7_SSL_INT_CONST(OPENSSL_CIPHER_AES_128_CBC,5)
PH7_SSL_INT_CONST(OPENSSL_CIPHER_AES_192_CBC,6)
PH7_SSL_INT_CONST(OPENSSL_CIPHER_AES_256_CBC,7)
PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_RSA,0)
PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_DSA,1)
PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_DH,2)
PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_EC,3)
PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_X25519,4)
PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_ED25519,5)
PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_X448,6)
PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_ED448,7)
PH7_SSL_INT_CONST(OPENSSL_RAW_DATA,1)
PH7_SSL_INT_CONST(OPENSSL_ZERO_PADDING,2)
PH7_SSL_INT_CONST(OPENSSL_DONT_ZERO_PAD_KEY,4)
PH7_SSL_INT_CONST(OPENSSL_TLSEXT_SERVER_NAME,1)
/*
 * stream_socket_enable_crypto()'s $crypto_method, and the ssl:// context's
 * `crypto_method`. php's OWN numbering, not OpenSSL's: bit 0 says CLIENT and
 * the rest are one bit per protocol (2 SSLv2, 4 SSLv3, 8 TLSv1.0, 16 TLSv1.1,
 * 32 TLSv1.2, 64 TLSv1.3), so a method is a SET of protocols a handshake may
 * settle on and the engine turns it into OpenSSL's min/max version pair.
 * Two consequences a script can see: `TLS_SERVER` and `SSLv23_SERVER` are the
 * SAME number (120) because php numbers the server side without the client
 * bit and the two sets coincide, and the SSLv2/SSLv3 bits still exist though
 * no OpenSSL 3 build will negotiate either.
 */
PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_SSLv2_CLIENT,3)
PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_SSLv2_SERVER,2)
PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_SSLv3_CLIENT,5)
PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_SSLv3_SERVER,4)
PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_SSLv23_CLIENT,57)
PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_SSLv23_SERVER,120)
PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLS_CLIENT,121)
PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLS_SERVER,120)
PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_0_CLIENT,9)
PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_0_SERVER,8)
PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_1_CLIENT,17)
PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_1_SERVER,16)
PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_2_CLIENT,33)
PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_2_SERVER,32)
PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_3_CLIENT,65)
PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_3_SERVER,64)
PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_ANY_CLIENT,127)
PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_ANY_SERVER,126)
PH7_SSL_INT_CONST(STREAM_CRYPTO_PROTO_SSLv3,4)
PH7_SSL_INT_CONST(STREAM_CRYPTO_PROTO_TLSv1_0,8)
PH7_SSL_INT_CONST(STREAM_CRYPTO_PROTO_TLSv1_1,16)
PH7_SSL_INT_CONST(STREAM_CRYPTO_PROTO_TLSv1_2,32)
PH7_SSL_INT_CONST(STREAM_CRYPTO_PROTO_TLSv1_3,64)
PH7_SSL_INT_CONST(OPENSSL_ENCODING_DER,0)
PH7_SSL_INT_CONST(OPENSSL_ENCODING_SMIME,1)
PH7_SSL_INT_CONST(OPENSSL_ENCODING_PEM,2)
static void PH7_OPENSSL_VERSION_TEXT_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_string(pVal,OPENSSL_VERSION_TEXT,-1);
}
static void PH7_OPENSSL_DEFAULT_STREAM_CIPHERS_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_string(pVal,
		"ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:"
		"ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:"
		"DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:"
		"ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:"
		"ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:"
		"ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:"
		"DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:"
		"DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:"
		"AES128:AES256:HIGH:!SSLv2:!aNULL:!eNULL:!EXPORT:!DES:!MD5:!RC4:!ADH",-1);
}
#endif /* PH7_ENABLE_OPENSSL */
PH7_FILEINFO_INT_CONST(FILEINFO_NONE,0)
PH7_FILEINFO_INT_CONST(FILEINFO_SYMLINK,2)
PH7_FILEINFO_INT_CONST(FILEINFO_MIME,1040)
PH7_FILEINFO_INT_CONST(FILEINFO_MIME_TYPE,16)
PH7_FILEINFO_INT_CONST(FILEINFO_MIME_ENCODING,1024)
PH7_FILEINFO_INT_CONST(FILEINFO_DEVICES,8)
PH7_FILEINFO_INT_CONST(FILEINFO_CONTINUE,32)
PH7_FILEINFO_INT_CONST(FILEINFO_PRESERVE_ATIME,128)
PH7_FILEINFO_INT_CONST(FILEINFO_RAW,256)
PH7_FILEINFO_INT_CONST(FILEINFO_APPLE,2048)
PH7_FILEINFO_INT_CONST(FILEINFO_EXTENSION,16777216)
/*
 * Table of built-in constants.
 */
static const ph7_builtin_constant aBuiltIn[] = {
	{"PH7_VERSION",          PH7_VER_Const      },
	{"PH7_ENGINE",           PH7_VER_Const      },
	{"__PH7__",              PH7_VER_Const      },
	{"PHP_VERSION",          PH7_PHPVerConst    },
	{"PHP_MAJOR_VERSION",    PH7_PHPMajorConst  },
	{"PHP_MINOR_VERSION",    PH7_PHPMinorConst  },
	{"PHP_RELEASE_VERSION",  PH7_PHPReleaseConst},
	{"PHP_EXTRA_VERSION",    PH7_PHPExtraConst  },
	{"PHP_VERSION_ID",       PH7_PHPVerIdConst  },
	{"PHP_OS",               PH7_OS_Const       },
	{"PHP_OS_FAMILY",        PH7_OS_FAMILY_Const},
	{"PHP_SAPI",             PH7_SAPI_Const     },
	{"PHP_EOL",              PH7_EOL_Const      },
	{"PHP_SESSION_DISABLED", PH7_PHP_SESSION_DISABLED_Const },
	{"PHP_SESSION_NONE",     PH7_PHP_SESSION_NONE_Const },
	{"PHP_SESSION_ACTIVE",   PH7_PHP_SESSION_ACTIVE_Const },
	{"INI_USER",             PH7_INI_USER_Const },
	{"INI_PERDIR",           PH7_INI_PERDIR_Const },
	{"INI_SYSTEM",           PH7_INI_SYSTEM_Const },
	{"INI_ALL",              PH7_INI_ALL_Const },
	{"MB_CASE_UPPER",        PH7_MB_CASE_UPPER_Const },
	{"MB_CASE_LOWER",        PH7_MB_CASE_LOWER_Const },
	{"MB_CASE_TITLE",        PH7_MB_CASE_TITLE_Const },
	{"PASSWORD_BCRYPT",      PH7_PASSWORD_BCRYPT_Const },
	{"PASSWORD_DEFAULT",     PH7_PASSWORD_BCRYPT_Const },
	{"PASSWORD_BCRYPT_DEFAULT_COST", PH7_PASSWORD_COST_Const },
	{"PASSWORD_ARGON2I",     PH7_PASSWORD_ARGON2I_Const },
	{"PASSWORD_ARGON2ID",    PH7_PASSWORD_ARGON2ID_Const },
	{"PASSWORD_ARGON2_DEFAULT_MEMORY_COST", PH7_ARGON2_MEM_Const },
	{"PASSWORD_ARGON2_DEFAULT_TIME_COST",   PH7_ARGON2_TIME_Const },
	{"PASSWORD_ARGON2_DEFAULT_THREADS",     PH7_ARGON2_THREADS_Const },
	{"FILTER_DEFAULT",              PH7_FILTER_DEFAULT_Const },
	{"FILTER_UNSAFE_RAW",           PH7_FILTER_UNSAFE_RAW_Const },
	{"FILTER_VALIDATE_INT",         PH7_FILTER_VALIDATE_INT_Const },
	{"FILTER_VALIDATE_BOOLEAN",     PH7_FILTER_VALIDATE_BOOLEAN_Const },
	{"FILTER_VALIDATE_BOOL",        PH7_FILTER_VALIDATE_BOOLEAN_Const },
	{"FILTER_VALIDATE_FLOAT",       PH7_FILTER_VALIDATE_FLOAT_Const },
	{"FILTER_VALIDATE_REGEXP",      PH7_FILTER_VALIDATE_REGEXP_Const },
	{"FILTER_VALIDATE_DOMAIN",      PH7_FILTER_VALIDATE_DOMAIN_Const },
	{"FILTER_VALIDATE_URL",         PH7_FILTER_VALIDATE_URL_Const },
	{"FILTER_VALIDATE_EMAIL",       PH7_FILTER_VALIDATE_EMAIL_Const },
	{"FILTER_VALIDATE_IP",          PH7_FILTER_VALIDATE_IP_Const },
	{"FILTER_VALIDATE_MAC",         PH7_FILTER_VALIDATE_MAC_Const },
	{"FILTER_CALLBACK",             PH7_FILTER_CALLBACK_Const },
	{"FILTER_THROW_ON_FAILURE",     PH7_FILTER_THROW_ON_FAILURE_Const },
	{"FILTER_SANITIZE_ENCODED",     PH7_FILTER_SANITIZE_ENCODED_Const },
	{"FILTER_SANITIZE_ADD_SLASHES", PH7_FILTER_SANITIZE_ADD_SLASHES_Const },
	{"FILTER_FLAG_NONE",            PH7_FILTER_FLAG_NONE_Const },
	{"FILTER_FLAG_EMPTY_STRING_NULL", PH7_FILTER_FLAG_EMPTY_STRING_NULL_Const },
	{"FILTER_SANITIZE_NUMBER_INT",  PH7_FILTER_SANITIZE_NUMBER_INT_Const },
	{"FILTER_SANITIZE_NUMBER_FLOAT",PH7_FILTER_SANITIZE_NUMBER_FLOAT_Const },
	{"FILTER_SANITIZE_SPECIAL_CHARS",PH7_FILTER_SANITIZE_SPECIAL_CHARS_Const },
	{"FILTER_SANITIZE_FULL_SPECIAL_CHARS",PH7_FILTER_SANITIZE_FULL_SPECIAL_CHARS_Const },
	{"FILTER_SANITIZE_EMAIL",       PH7_FILTER_SANITIZE_EMAIL_Const },
	{"FILTER_SANITIZE_URL",         PH7_FILTER_SANITIZE_URL_Const },
	{"FILTER_FLAG_ALLOW_OCTAL",     PH7_FILTER_FLAG_ALLOW_OCTAL_Const },
	{"FILTER_FLAG_ALLOW_HEX",       PH7_FILTER_FLAG_ALLOW_HEX_Const },
	{"FILTER_FLAG_STRIP_LOW",       PH7_FILTER_FLAG_STRIP_LOW_Const },
	{"FILTER_FLAG_STRIP_HIGH",      PH7_FILTER_FLAG_STRIP_HIGH_Const },
	{"FILTER_FLAG_ENCODE_LOW",      PH7_FILTER_FLAG_ENCODE_LOW_Const },
	{"FILTER_FLAG_ENCODE_HIGH",     PH7_FILTER_FLAG_ENCODE_HIGH_Const },
	{"FILTER_FLAG_ENCODE_AMP",      PH7_FILTER_FLAG_ENCODE_AMP_Const },
	{"FILTER_FLAG_NO_ENCODE_QUOTES",PH7_FILTER_FLAG_NO_ENCODE_QUOTES_Const },
	{"FILTER_FLAG_STRIP_BACKTICK",  PH7_FILTER_FLAG_STRIP_BACKTICK_Const },
	{"FILTER_FLAG_ALLOW_FRACTION",  PH7_FILTER_FLAG_ALLOW_FRACTION_Const },
	{"FILTER_FLAG_ALLOW_THOUSAND",  PH7_FILTER_FLAG_ALLOW_THOUSAND_Const },
	{"FILTER_FLAG_ALLOW_SCIENTIFIC",PH7_FILTER_FLAG_ALLOW_SCIENTIFIC_Const },
	{"FILTER_FLAG_IPV4",            PH7_FILTER_FLAG_IPV4_Const },
	{"FILTER_FLAG_IPV6",            PH7_FILTER_FLAG_IPV6_Const },
	{"FILTER_FLAG_PATH_REQUIRED",   PH7_FILTER_FLAG_PATH_REQUIRED_Const },
	{"FILTER_FLAG_QUERY_REQUIRED",  PH7_FILTER_FLAG_QUERY_REQUIRED_Const },
	{"FILTER_FLAG_HOSTNAME",        PH7_FILTER_FLAG_HOSTNAME_Const },
	{"FILTER_FLAG_EMAIL_UNICODE",   PH7_FILTER_FLAG_EMAIL_UNICODE_Const },
	{"FILTER_FLAG_NO_RES_RANGE",    PH7_FILTER_FLAG_NO_RES_RANGE_Const },
	{"FILTER_FLAG_NO_PRIV_RANGE",   PH7_FILTER_FLAG_NO_PRIV_RANGE_Const },
	{"FILTER_FLAG_GLOBAL_RANGE",    PH7_FILTER_FLAG_GLOBAL_RANGE_Const },
	{"FILTER_REQUIRE_ARRAY",        PH7_FILTER_REQUIRE_ARRAY_Const },
	{"FILTER_REQUIRE_SCALAR",       PH7_FILTER_REQUIRE_SCALAR_Const },
	{"FILTER_FORCE_ARRAY",          PH7_FILTER_FORCE_ARRAY_Const },
	{"FILTER_NULL_ON_FAILURE",      PH7_FILTER_NULL_ON_FAILURE_Const },
	{"INPUT_POST",                  PH7_INPUT_POST_Const },
	{"INPUT_GET",                   PH7_INPUT_GET_Const },
	{"INPUT_COOKIE",                PH7_INPUT_COOKIE_Const },
	{"INPUT_ENV",                   PH7_INPUT_ENV_Const },
	{"INPUT_SERVER",                PH7_INPUT_SERVER_Const },
	{"CAL_GREGORIAN",        PH7_CAL_GREGORIAN_Const },
	{"CAL_JULIAN",           PH7_CAL_JULIAN_Const    },
	{"CAL_JEWISH",           PH7_CAL_JEWISH_Const    },
	{"CAL_FRENCH",           PH7_CAL_FRENCH_Const    },
	{"CAL_NUM_CALS",         PH7_CAL_NUM_CALS_Const  },
	{"CAL_DOW_DAYNO",        PH7_CAL_DOW_DAYNO_Const },
	{"CAL_DOW_LONG",         PH7_CAL_DOW_LONG_Const  },
	{"CAL_DOW_SHORT",        PH7_CAL_DOW_SHORT_Const },
	{"CAL_MONTH_GREGORIAN_SHORT", PH7_CAL_MONTH_GREGORIAN_SHORT_Const },
	{"CAL_MONTH_GREGORIAN_LONG",  PH7_CAL_MONTH_GREGORIAN_LONG_Const },
	{"CAL_MONTH_JULIAN_SHORT",    PH7_CAL_MONTH_JULIAN_SHORT_Const },
	{"CAL_MONTH_JULIAN_LONG",     PH7_CAL_MONTH_JULIAN_LONG_Const },
	{"CAL_MONTH_JEWISH",          PH7_CAL_MONTH_JEWISH_Const },
	{"CAL_MONTH_FRENCH",          PH7_CAL_MONTH_FRENCH_Const },
	{"CAL_EASTER_DEFAULT",   PH7_CAL_EASTER_DEFAULT_Const },
	{"CAL_EASTER_ROMAN",     PH7_CAL_EASTER_ROMAN_Const   },
	{"CAL_EASTER_ALWAYS_GREGORIAN", PH7_CAL_EASTER_ALWAYS_GREGORIAN_Const },
	{"CAL_EASTER_ALWAYS_JULIAN",    PH7_CAL_EASTER_ALWAYS_JULIAN_Const },
	{"CAL_JEWISH_ADD_ALAFIM_GERESH", PH7_CAL_JEWISH_ADD_ALAFIM_GERESH_Const },
	{"CAL_JEWISH_ADD_ALAFIM",        PH7_CAL_JEWISH_ADD_ALAFIM_Const },
	{"CAL_JEWISH_ADD_GERESHAYIM",    PH7_CAL_JEWISH_ADD_GERESHAYIM_Const },
	{"IMAGETYPE_GIF",        PH7_IMAGETYPE_GIF_Const     },
	{"IMAGETYPE_JPEG",       PH7_IMAGETYPE_JPEG_Const    },
	{"IMAGETYPE_PNG",        PH7_IMAGETYPE_PNG_Const     },
	{"IMAGETYPE_SWF",        PH7_IMAGETYPE_SWF_Const     },
	{"IMAGETYPE_PSD",        PH7_IMAGETYPE_PSD_Const     },
	{"IMAGETYPE_BMP",        PH7_IMAGETYPE_BMP_Const     },
	{"IMAGETYPE_TIFF_II",    PH7_IMAGETYPE_TIFF_II_Const },
	{"IMAGETYPE_TIFF_MM",    PH7_IMAGETYPE_TIFF_MM_Const },
	{"IMAGETYPE_JPC",        PH7_IMAGETYPE_JPC_Const     },
	{"IMAGETYPE_JP2",        PH7_IMAGETYPE_JP2_Const     },
	{"IMAGETYPE_JPX",        PH7_IMAGETYPE_JPX_Const     },
	{"IMAGETYPE_JB2",        PH7_IMAGETYPE_JB2_Const     },
	{"IMAGETYPE_SWC",        PH7_IMAGETYPE_SWC_Const     },
	{"IMAGETYPE_IFF",        PH7_IMAGETYPE_IFF_Const     },
	{"IMAGETYPE_WBMP",       PH7_IMAGETYPE_WBMP_Const    },
	/* php's own alias row: the same 9 the JPC name expands to. */
	{"IMAGETYPE_JPEG2000",   PH7_IMAGETYPE_JPC_Const     },
	{"IMAGETYPE_XBM",        PH7_IMAGETYPE_XBM_Const     },
	{"IMAGETYPE_ICO",        PH7_IMAGETYPE_ICO_Const     },
	{"IMAGETYPE_WEBP",       PH7_IMAGETYPE_WEBP_Const    },
	{"IMAGETYPE_AVIF",       PH7_IMAGETYPE_AVIF_Const    },
	{"IMAGETYPE_HEIF",       PH7_IMAGETYPE_HEIF_Const    },
	{"IMAGETYPE_UNKNOWN",    PH7_IMAGETYPE_UNKNOWN_Const },
	{"IMAGETYPE_COUNT",      PH7_IMAGETYPE_COUNT_Const   },
#ifdef PH7_ENABLE_LIBXML
	{"IMAGETYPE_SVG",        PH7_IMAGETYPE_SVG_Const     },
#endif
	{"PHP_INT_MAX",          PH7_INTMAX_Const   },
	{"MAXINT",               PH7_INTMAX_Const   },
	{"PHP_INT_MIN",          PH7_INTMIN_Const   },
	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },
	{"PHP_FLOAT_EPSILON",    PH7_FLOATEPSILON_Const },
	{"PHP_FLOAT_MAX",        PH7_FLOATMAX_Const },
	{"PHP_FLOAT_MIN",        PH7_FLOATMIN_Const },
	{"PHP_FLOAT_DIG",        PH7_FLOATDIG_Const },
	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },
	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },
	{"DIR_SEP",              PH7_DIRSEP_Const   },
	{"__TIME__",             PH7_TIME_Const     },
	{"__DATE__",             PH7_DATE_Const     },
	{"__FILE__",             PH7_FILE_Const     },
	{"__DIR__",              PH7_DIR_Const      },
	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },
	{"E_ERROR",              PH7_E_ERROR_Const  },
	{"E_WARNING",            PH7_E_WARNING_Const},
	{"E_PARSE",              PH7_E_PARSE_Const  },
	{"E_NOTICE",             PH7_E_NOTICE_Const },
	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },
	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },
	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },
	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },
	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },
	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },
	{"E_USER_NOTICE",        PH7_E_USER_NOTICE_Const   },
	{"E_STRICT",             PH7_E_STRICT_Const        },
	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },
	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },
	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },
	{"E_ALL",                PH7_E_ALL_Const              },
	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },
	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },
	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },
	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},
	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },
	{"STREAM_IS_URL",                PH7_STREAM_IS_URL_Const },
	{"STREAM_USE_PATH",              PH7_STREAM_USE_PATH_Const },
	{"STREAM_IGNORE_URL",            PH7_STREAM_IGNORE_URL_Const },
	{"STREAM_REPORT_ERRORS",         PH7_STREAM_REPORT_ERRORS_Const },
	{"STREAM_MUST_SEEK",             PH7_STREAM_MUST_SEEK_Const },
	{"STREAM_URL_STAT_LINK",         PH7_STREAM_URL_STAT_LINK_Const },
	{"STREAM_URL_STAT_QUIET",        PH7_STREAM_URL_STAT_QUIET_Const },
	{"STREAM_MKDIR_RECURSIVE",       PH7_STREAM_MKDIR_RECURSIVE_Const },
	{"STREAM_META_TOUCH",            PH7_STREAM_META_TOUCH_Const },
	{"STREAM_META_OWNER_NAME",       PH7_STREAM_META_OWNER_NAME_Const },
	{"STREAM_META_OWNER",            PH7_STREAM_META_OWNER_Const },
	{"STREAM_META_GROUP_NAME",       PH7_STREAM_META_GROUP_NAME_Const },
	{"STREAM_META_GROUP",            PH7_STREAM_META_GROUP_Const },
	{"STREAM_META_ACCESS",           PH7_STREAM_META_ACCESS_Const },
	{"STREAM_OPTION_BLOCKING",       PH7_STREAM_OPTION_BLOCKING_Const },
	{"STREAM_OPTION_READ_BUFFER",    PH7_STREAM_OPTION_READ_BUFFER_Const },
	{"STREAM_OPTION_WRITE_BUFFER",   PH7_STREAM_OPTION_WRITE_BUFFER_Const },
	{"STREAM_OPTION_READ_TIMEOUT",   PH7_STREAM_OPTION_READ_TIMEOUT_Const },
	{"STREAM_BUFFER_NONE",           PH7_STREAM_BUFFER_NONE_Const },
	{"STREAM_BUFFER_LINE",           PH7_STREAM_BUFFER_LINE_Const },
	{"STREAM_BUFFER_FULL",           PH7_STREAM_BUFFER_FULL_Const },
	{"STREAM_CAST_AS_STREAM",        PH7_STREAM_CAST_AS_STREAM_Const },
	{"STREAM_CAST_FOR_SELECT",       PH7_STREAM_CAST_FOR_SELECT_Const },
	{"STREAM_SERVER_BIND",           PH7_STREAM_SERVER_BIND_Const },
	{"STREAM_SERVER_LISTEN",         PH7_STREAM_SERVER_LISTEN_Const },
	{"STREAM_CLIENT_CONNECT",        PH7_STREAM_CLIENT_CONNECT_Const },
	{"STREAM_CLIENT_ASYNC_CONNECT",  PH7_STREAM_CLIENT_ASYNC_CONNECT_Const },
	{"STREAM_CLIENT_PERSISTENT",     PH7_STREAM_CLIENT_PERSISTENT_Const },
	{"PSFS_PASS_ON",                 PH7_PSFS_PASS_ON_Const },
	{"PSFS_FEED_ME",                 PH7_PSFS_FEED_ME_Const },
	{"PSFS_ERR_FATAL",               PH7_PSFS_ERR_FATAL_Const },
	{"PSFS_FLAG_NORMAL",             PH7_PSFS_FLAG_NORMAL_Const },
	{"PSFS_FLAG_FLUSH_INC",          PH7_PSFS_FLAG_FLUSH_INC_Const },
	{"PSFS_FLAG_FLUSH_CLOSE",        PH7_PSFS_FLAG_FLUSH_CLOSE_Const },
	{"STREAM_NOTIFY_RESOLVE",        PH7_STREAM_NOTIFY_RESOLVE_Const },
	{"STREAM_NOTIFY_CONNECT",        PH7_STREAM_NOTIFY_CONNECT_Const },
	{"STREAM_NOTIFY_AUTH_REQUIRED",  PH7_STREAM_NOTIFY_AUTH_REQUIRED_Const },
	{"STREAM_NOTIFY_MIME_TYPE_IS",   PH7_STREAM_NOTIFY_MIME_TYPE_IS_Const },
	{"STREAM_NOTIFY_FILE_SIZE_IS",   PH7_STREAM_NOTIFY_FILE_SIZE_IS_Const },
	{"STREAM_NOTIFY_REDIRECTED",     PH7_STREAM_NOTIFY_REDIRECTED_Const },
	{"STREAM_NOTIFY_PROGRESS",       PH7_STREAM_NOTIFY_PROGRESS_Const },
	{"STREAM_NOTIFY_COMPLETED",      PH7_STREAM_NOTIFY_COMPLETED_Const },
	{"STREAM_NOTIFY_FAILURE",        PH7_STREAM_NOTIFY_FAILURE_Const },
	{"STREAM_NOTIFY_AUTH_RESULT",    PH7_STREAM_NOTIFY_AUTH_RESULT_Const },
	{"STREAM_NOTIFY_SEVERITY_INFO",  PH7_STREAM_NOTIFY_SEVERITY_INFO_Const },
	{"STREAM_NOTIFY_SEVERITY_WARN",  PH7_STREAM_NOTIFY_SEVERITY_WARN_Const },
	{"STREAM_NOTIFY_SEVERITY_ERR",   PH7_STREAM_NOTIFY_SEVERITY_ERR_Const },
	{"STREAM_FILTER_READ",           PH7_STREAM_FILTER_READ_Const },
	{"STREAM_FILTER_WRITE",          PH7_STREAM_FILTER_WRITE_Const },
	{"STREAM_FILTER_ALL",            PH7_STREAM_FILTER_ALL_Const },
	{"STREAM_SHUT_RD",               PH7_STREAM_SHUT_RD_Const },
	{"STREAM_SHUT_WR",               PH7_STREAM_SHUT_WR_Const },
	{"STREAM_SHUT_RDWR",             PH7_STREAM_SHUT_RDWR_Const },
	{"STREAM_OOB",                   PH7_STREAM_OOB_Const },
	{"STREAM_PEEK",                  PH7_STREAM_PEEK_Const },
#ifdef PH7_ENABLE_NET
	{"STREAM_PF_INET",               PH7_STREAM_PF_INET_Const },
	{"STREAM_PF_INET6",              PH7_STREAM_PF_INET6_Const },
	{"STREAM_PF_UNIX",               PH7_STREAM_PF_UNIX_Const },
	{"STREAM_SOCK_STREAM",           PH7_STREAM_SOCK_STREAM_Const },
	{"STREAM_SOCK_DGRAM",            PH7_STREAM_SOCK_DGRAM_Const },
	{"STREAM_SOCK_RAW",              PH7_STREAM_SOCK_RAW_Const },
	{"STREAM_SOCK_SEQPACKET",        PH7_STREAM_SOCK_SEQPACKET_Const },
	{"STREAM_SOCK_RDM",              PH7_STREAM_SOCK_RDM_Const },
	{"STREAM_IPPROTO_IP",            PH7_STREAM_IPPROTO_IP_Const },
	{"STREAM_IPPROTO_TCP",           PH7_STREAM_IPPROTO_TCP_Const },
	{"STREAM_IPPROTO_UDP",           PH7_STREAM_IPPROTO_UDP_Const },
	{"STREAM_IPPROTO_ICMP",          PH7_STREAM_IPPROTO_ICMP_Const },
	{"STREAM_IPPROTO_RAW",           PH7_STREAM_IPPROTO_RAW_Const },
#endif
	{"MT_RAND_MT19937",              PH7_MT_RAND_MT19937_Const },
	{"MT_RAND_PHP",                  PH7_MT_RAND_PHP_Const  },
	{"PHP_OUTPUT_HANDLER_WRITE",     PH7_OB_WRITE_Const     },
	{"PHP_OUTPUT_HANDLER_CONT",      PH7_OB_WRITE_Const     },
	{"PHP_OUTPUT_HANDLER_START",     PH7_OB_START_Const     },
	{"PHP_OUTPUT_HANDLER_CLEAN",     PH7_OB_CLEAN_Const     },
	{"PHP_OUTPUT_HANDLER_FLUSH",     PH7_OB_FLUSH_Const     },
	{"PHP_OUTPUT_HANDLER_FINAL",     PH7_OB_FINAL_Const     },
	{"PHP_OUTPUT_HANDLER_END",       PH7_OB_FINAL_Const     },
	{"PHP_OUTPUT_HANDLER_CLEANABLE", PH7_OB_CLEANABLE_Const },
	{"PHP_OUTPUT_HANDLER_FLUSHABLE", PH7_OB_FLUSHABLE_Const },
	{"PHP_OUTPUT_HANDLER_REMOVABLE", PH7_OB_REMOVABLE_Const },
	{"PHP_OUTPUT_HANDLER_STDFLAGS",  PH7_OB_STDFLAGS_Const  },
	{"PHP_OUTPUT_HANDLER_STARTED",   PH7_OB_STARTED_Const   },
	{"PHP_OUTPUT_HANDLER_DISABLED",  PH7_OB_DISABLED_Const  },
	{"PHP_OUTPUT_HANDLER_PROCESSED", PH7_OB_PROCESSED_Const },
	{"CONNECTION_NORMAL",    PH7_CONNECTION_NORMAL_Const  },
	{"CONNECTION_ABORTED",   PH7_CONNECTION_ABORTED_Const },
	{"CONNECTION_TIMEOUT",   PH7_CONNECTION_TIMEOUT_Const },
	{"ARRAY_FILTER_USE_KEY", PH7_ARRAY_FILTER_USE_KEY_Const },
	{"ARRAY_FILTER_USE_BOTH",PH7_ARRAY_FILTER_USE_BOTH_Const},
	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },
	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },
	{"SORT_ASC",             PH7_SORT_ASC_Const     },
	{"SORT_DESC",            PH7_SORT_DESC_Const    },
	{"SORT_REGULAR",         PH7_SORT_REG_Const     },
	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },
	{"SORT_STRING",          PH7_SORT_STRING_Const  },
	{"SORT_LOCALE_STRING",   PH7_SORT_LOCALE_STRING_Const },
	{"SORT_NATURAL",         PH7_SORT_NATURAL_Const },
	{"SORT_FLAG_CASE",       PH7_SORT_FLAG_CASE_Const },
	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },
	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },
	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },
	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },
	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },
	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},
#ifdef PH7_ENABLE_MATH_FUNC
	{"M_PI",                 PH7_M_PI_Const         },
	{"M_E",                  PH7_M_E_Const          },
	{"M_LOG2E",              PH7_M_LOG2E_Const      },
	{"M_LOG10E",             PH7_M_LOG10E_Const     },
	{"M_LN2",                PH7_M_LN2_Const        },
	{"M_LN10",               PH7_M_LN10_Const       },
	{"M_PI_2",               PH7_M_PI_2_Const       },
	{"M_PI_4",               PH7_M_PI_4_Const       },
	{"M_1_PI",               PH7_M_1_PI_Const       },
	{"M_2_PI",               PH7_M_2_PI_Const       },
	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },
	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },
	{"M_SQRT2",              PH7_M_SQRT2_Const      },
	{"M_SQRT3",              PH7_M_SQRT3_Const      },
	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },
	{"M_LNPI",               PH7_M_LNPI_Const       },
	{"M_EULER",              PH7_M_EULER_Const      },
	{"NAN",                  PH7_NAN_Const          },
	{"INF",                  PH7_INF_Const          },
#endif /* PH7_ENABLE_MATH_FUNC */
	{"SUNFUNCS_RET_TIMESTAMP", PH7_SUNFUNCS_RET_TIMESTAMP_Const },
	{"SUNFUNCS_RET_STRING",  PH7_SUNFUNCS_RET_STRING_Const },
	{"SUNFUNCS_RET_DOUBLE",  PH7_SUNFUNCS_RET_DOUBLE_Const },
	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },
	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },
	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },
	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },
	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },
	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },
	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },
	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },
	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },
	{"DATE_RFC3339_EXTENDED",PH7_DATE_RFC3339_EXTENDED_Const },
	{"DATE_RFC7231",         PH7_DATE_RFC7231_Const },
	{"DATE_ISO8601_EXPANDED",PH7_DATE_ISO8601_EXPANDED_Const },
	{"DATE_RSS",             PH7_DATE_RSS_Const     },
	{"DATE_W3C",             PH7_DATE_W3C_Const     },
	{"FILE_TEXT",            PH7_FILE_TEXT_Const    },
	{"FILE_BINARY",          PH7_FILE_TEXT_Const    },
	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },
	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },
	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },
	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },
	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},
	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},
	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },
	{"ENT_XML1",             PH7_ENT_XML1_Const     },
	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },
	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },
	{"ISO-8859-1",           PH7_ISO88591_Const     },
	{"ISO_8859_1",           PH7_ISO88591_Const     },
	{"UTF-8",                PH7_UTF8_Const         },
	{"UTF8",                 PH7_UTF8_Const         },
	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},
	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },
	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},
	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},
	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},
	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},
	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},
	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},
	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},
	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},
	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},
	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},
	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },
	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },
	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },
	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },
	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },
	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },
	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},
	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },
	{"PATHINFO_ALL",         PH7_PATHINFO_ALL_Const },
	/* ASSERT_QUIET_EVAL was REMOVED in php 8.0: referencing it is an Error there */
#ifdef PH7_ENABLE_PCRE
	{"PCRE_VERSION",         PH7_PCRE_VERSION_Const  },
	{"PCRE_VERSION_MAJOR",   PH7_PCRE_VERSION_MAJOR_Const },
	{"PCRE_VERSION_MINOR",   PH7_PCRE_VERSION_MINOR_Const },
	{"PCRE_JIT_SUPPORT",     PH7_PCRE_JIT_SUPPORT_Const },
#endif
	{"ZEND_THREAD_SAFE",     PH7_ZEND_THREAD_SAFE_Const },
	{"ZEND_DEBUG_BUILD",     PH7_ZEND_DEBUG_BUILD_Const },
	{"PHP_ZTS",              PH7_PHP_ZTS_Const       },
	{"PHP_DEBUG",            PH7_PHP_DEBUG_Const     },
	{"INFO_GENERAL",         PH7_INFO_GENERAL_Const  },
	{"INFO_CREDITS",         PH7_INFO_CREDITS_Const  },
	{"INFO_CONFIGURATION",   PH7_INFO_CONFIGURATION_Const },
	{"INFO_MODULES",         PH7_INFO_MODULES_Const  },
	{"INFO_ENVIRONMENT",     PH7_INFO_ENVIRONMENT_Const },
	{"INFO_VARIABLES",       PH7_INFO_VARIABLES_Const },
	{"INFO_LICENSE",         PH7_INFO_LICENSE_Const  },
	{"INFO_ALL",             PH7_INFO_ALL_Const      },
	{"LC_CTYPE",             PH7_LC_CTYPE_Const      },
	{"LC_NUMERIC",           PH7_LC_NUMERIC_Const    },
	{"LC_TIME",              PH7_LC_TIME_Const       },
	{"LC_COLLATE",           PH7_LC_COLLATE_Const    },
	{"LC_MONETARY",          PH7_LC_MONETARY_Const   },
	{"LC_MESSAGES",          PH7_LC_MESSAGES_Const   },
	{"LC_ALL",               PH7_LC_ALL_Const        },
#ifndef __WINNT__
	/* ext/posix */
#ifdef F_OK
	{"POSIX_F_OK", PH7_POSIX_F_OK_Const },
#endif
#ifdef X_OK
	{"POSIX_X_OK", PH7_POSIX_X_OK_Const },
#endif
#ifdef W_OK
	{"POSIX_W_OK", PH7_POSIX_W_OK_Const },
#endif
#ifdef R_OK
	{"POSIX_R_OK", PH7_POSIX_R_OK_Const },
#endif
#ifdef S_IFREG
	{"POSIX_S_IFREG", PH7_POSIX_S_IFREG_Const },
#endif
#ifdef S_IFCHR
	{"POSIX_S_IFCHR", PH7_POSIX_S_IFCHR_Const },
#endif
#ifdef S_IFBLK
	{"POSIX_S_IFBLK", PH7_POSIX_S_IFBLK_Const },
#endif
#ifdef S_IFIFO
	{"POSIX_S_IFIFO", PH7_POSIX_S_IFIFO_Const },
#endif
#ifdef S_IFSOCK
	{"POSIX_S_IFSOCK", PH7_POSIX_S_IFSOCK_Const },
#endif
#ifdef RLIMIT_AS
	{"POSIX_RLIMIT_AS", PH7_POSIX_RLIMIT_AS_Const },
#endif
#ifdef RLIMIT_CORE
	{"POSIX_RLIMIT_CORE", PH7_POSIX_RLIMIT_CORE_Const },
#endif
#ifdef RLIMIT_CPU
	{"POSIX_RLIMIT_CPU", PH7_POSIX_RLIMIT_CPU_Const },
#endif
#ifdef RLIMIT_DATA
	{"POSIX_RLIMIT_DATA", PH7_POSIX_RLIMIT_DATA_Const },
#endif
#ifdef RLIMIT_FSIZE
	{"POSIX_RLIMIT_FSIZE", PH7_POSIX_RLIMIT_FSIZE_Const },
#endif
#ifdef RLIMIT_LOCKS
	{"POSIX_RLIMIT_LOCKS", PH7_POSIX_RLIMIT_LOCKS_Const },
#endif
#ifdef RLIMIT_MEMLOCK
	{"POSIX_RLIMIT_MEMLOCK", PH7_POSIX_RLIMIT_MEMLOCK_Const },
#endif
#ifdef RLIMIT_MSGQUEUE
	{"POSIX_RLIMIT_MSGQUEUE", PH7_POSIX_RLIMIT_MSGQUEUE_Const },
#endif
#ifdef RLIMIT_NICE
	{"POSIX_RLIMIT_NICE", PH7_POSIX_RLIMIT_NICE_Const },
#endif
#ifdef RLIMIT_NOFILE
	{"POSIX_RLIMIT_NOFILE", PH7_POSIX_RLIMIT_NOFILE_Const },
#endif
#ifdef RLIMIT_NPROC
	{"POSIX_RLIMIT_NPROC", PH7_POSIX_RLIMIT_NPROC_Const },
#endif
#ifdef RLIMIT_RSS
	{"POSIX_RLIMIT_RSS", PH7_POSIX_RLIMIT_RSS_Const },
#endif
#ifdef RLIMIT_RTPRIO
	{"POSIX_RLIMIT_RTPRIO", PH7_POSIX_RLIMIT_RTPRIO_Const },
#endif
#ifdef RLIMIT_RTTIME
	{"POSIX_RLIMIT_RTTIME", PH7_POSIX_RLIMIT_RTTIME_Const },
#endif
#ifdef RLIMIT_SIGPENDING
	{"POSIX_RLIMIT_SIGPENDING", PH7_POSIX_RLIMIT_SIGPENDING_Const },
#endif
#ifdef RLIMIT_STACK
	{"POSIX_RLIMIT_STACK", PH7_POSIX_RLIMIT_STACK_Const },
#endif
#ifdef _SC_ARG_MAX
	{"POSIX_SC_ARG_MAX", PH7_POSIX_SC_ARG_MAX_Const },
#endif
#ifdef _SC_CHILD_MAX
	{"POSIX_SC_CHILD_MAX", PH7_POSIX_SC_CHILD_MAX_Const },
#endif
#ifdef _SC_CLK_TCK
	{"POSIX_SC_CLK_TCK", PH7_POSIX_SC_CLK_TCK_Const },
#endif
#ifdef _SC_OPEN_MAX
	{"POSIX_SC_OPEN_MAX", PH7_POSIX_SC_OPEN_MAX_Const },
#endif
#ifdef _SC_PAGESIZE
	{"POSIX_SC_PAGESIZE", PH7_POSIX_SC_PAGESIZE_Const },
#endif
#ifdef _SC_NPROCESSORS_CONF
	{"POSIX_SC_NPROCESSORS_CONF", PH7_POSIX_SC_NPROCESSORS_CONF_Const },
#endif
#ifdef _SC_NPROCESSORS_ONLN
	{"POSIX_SC_NPROCESSORS_ONLN", PH7_POSIX_SC_NPROCESSORS_ONLN_Const },
#endif
#ifdef _PC_LINK_MAX
	{"POSIX_PC_LINK_MAX", PH7_POSIX_PC_LINK_MAX_Const },
#endif
#ifdef _PC_MAX_CANON
	{"POSIX_PC_MAX_CANON", PH7_POSIX_PC_MAX_CANON_Const },
#endif
#ifdef _PC_MAX_INPUT
	{"POSIX_PC_MAX_INPUT", PH7_POSIX_PC_MAX_INPUT_Const },
#endif
#ifdef _PC_NAME_MAX
	{"POSIX_PC_NAME_MAX", PH7_POSIX_PC_NAME_MAX_Const },
#endif
#ifdef _PC_PATH_MAX
	{"POSIX_PC_PATH_MAX", PH7_POSIX_PC_PATH_MAX_Const },
#endif
#ifdef _PC_PIPE_BUF
	{"POSIX_PC_PIPE_BUF", PH7_POSIX_PC_PIPE_BUF_Const },
#endif
#ifdef _PC_CHOWN_RESTRICTED
	{"POSIX_PC_CHOWN_RESTRICTED", PH7_POSIX_PC_CHOWN_RESTRICTED_Const },
#endif
#ifdef _PC_NO_TRUNC
	{"POSIX_PC_NO_TRUNC", PH7_POSIX_PC_NO_TRUNC_Const },
#endif
#ifdef _PC_ALLOC_SIZE_MIN
	{"POSIX_PC_ALLOC_SIZE_MIN", PH7_POSIX_PC_ALLOC_SIZE_MIN_Const },
#endif
#ifdef _PC_SYMLINK_MAX
	{"POSIX_PC_SYMLINK_MAX", PH7_POSIX_PC_SYMLINK_MAX_Const },
#endif
	{"POSIX_RLIMIT_INFINITY", PH7_POSIX_RLIMIT_INFINITY_Const },
#endif /* __WINNT__ */
	{"SEEK_SET",             PH7_SEEK_SET_Const      },
	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },
	{"SEEK_END",             PH7_SEEK_END_Const      },
	{"LOCK_EX",              PH7_LOCK_EX_Const      },
	{"LOCK_SH",              PH7_LOCK_SH_Const      },
	{"LOCK_NB",              PH7_LOCK_NB_Const      },
	{"LOCK_UN",              PH7_LOCK_UN_Const      },
	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},
	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},
	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},
	{"FILE_APPEND",           PH7_FILE_APPEND_Const },
	{"FILE_NO_DEFAULT_CONTEXT", PH7_FILE_NO_DEFAULT_CONTEXT_Const },
	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },
	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },
	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },
	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },
	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },
	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },
	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},
	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },
	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },
	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },
	{"GLOB_AVAILABLE_FLAGS", PH7_GLOB_AVAILABLE_FLAGS_Const },
	{"STDIN",                PH7_STDIN_Const        },
	{"stdin",                PH7_STDIN_Const        },
	{"STDOUT",               PH7_STDOUT_Const       },
	{"stdout",               PH7_STDOUT_Const       },
	{"STDERR",               PH7_STDERR_Const       },
	{"stderr",               PH7_STDERR_Const       },
	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },
	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },
	{"INI_SCANNER_TYPED",    PH7_INI_SCANNER_TYPED_Const  },
	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },
	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },
	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },
	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },
	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },
	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },
	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},
	{"EXTR_REFS",            PH7_EXTR_REFS_Const        },
#ifndef PH7_DISABLE_HASH_FUNC
	{"HASH_HMAC",              PH7_HASH_HMAC_Const},
	{"CRYPT_SALT_LENGTH",      PH7_CRYPT_SALT_LENGTH_Const},
	{"CRYPT_STD_DES",          PH7_CRYPT_ONE_Const},
	{"CRYPT_EXT_DES",          PH7_CRYPT_ONE_Const},
	{"CRYPT_MD5",              PH7_CRYPT_ONE_Const},
	{"CRYPT_BLOWFISH",         PH7_CRYPT_ONE_Const},
	{"CRYPT_SHA256",           PH7_CRYPT_ONE_Const},
	{"CRYPT_SHA512",           PH7_CRYPT_ONE_Const},
#endif
	{"ICONV_IMPL",             PH7_ICONV_IMPL_Const},
	{"ICONV_VERSION",          PH7_ICONV_VERSION_Const},
	{"ICONV_MIME_DECODE_STRICT", PH7_ICONV_MIME_DECODE_STRICT_Const},
	{"ICONV_MIME_DECODE_CONTINUE_ON_ERROR", PH7_ICONV_MIME_DECODE_CONTINUE_ON_ERROR_Const},
	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},
	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},
	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},
	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},
	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},
	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},
	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},
	{"JSON_OBJECT_AS_ARRAY",   PH7_JSON_OBJECT_AS_ARRAY_Const},
	{"JSON_PARTIAL_OUTPUT_ON_ERROR", PH7_JSON_PARTIAL_OUTPUT_ON_ERROR_Const},
	{"JSON_PRESERVE_ZERO_FRACTION",  PH7_JSON_PRESERVE_ZERO_FRACTION_Const},
	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},
	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},
	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},
	{"JSON_UNESCAPED_LINE_TERMINATORS", PH7_JSON_UNESCAPED_LINE_TERMINATORS_Const},
	{"JSON_INVALID_UTF8_IGNORE", PH7_JSON_INVALID_UTF8_IGNORE_Const},
	{"JSON_INVALID_UTF8_SUBSTITUTE", PH7_JSON_INVALID_UTF8_SUBSTITUTE_Const},
	{"JSON_THROW_ON_ERROR",    PH7_JSON_THROW_ON_ERROR_Const},
	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},
	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},
	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},
	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},
	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},
	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},
	{"JSON_ERROR_RECURSION", PH7_JSON_ERROR_RECURSION_Const},
	{"JSON_ERROR_UNSUPPORTED_TYPE", PH7_JSON_ERROR_UNSUPPORTED_TYPE_Const},
	{"JSON_ERROR_INVALID_PROPERTY_NAME", PH7_JSON_ERROR_INVALID_PROPERTY_NAME_Const},
	{"JSON_ERROR_UTF16",     PH7_JSON_ERROR_UTF16_Const},
	{"JSON_ERROR_NON_BACKED_ENUM", PH7_JSON_ERROR_NON_BACKED_ENUM_Const},
	{"JSON_ERROR_INF_OR_NAN", PH7_JSON_ERROR_INF_OR_NAN_Const},
	/* `self`, `parent` and `static` are KEYWORDS in php, not constants: using one as a bare
	 * word is an "Undefined constant" Error (or a parse error for `static`). PH7 registered
	 * them as constants that quietly expanded to the class name / NULL, so a typo'd bare
	 * word silently produced a value. The `self::`/`parent::`/`static::` forms are handled
	 * by the `::` compile path and do not go through the constant table. */
#ifdef PH7_ENABLE_ZLIB
	{"FORCE_GZIP",             PH7_FORCE_GZIP_Const },
	{"FORCE_DEFLATE",          PH7_FORCE_DEFLATE_Const },
	{"ZLIB_ENCODING_RAW",      PH7_ZLIB_ENCODING_RAW_Const },
	{"ZLIB_ENCODING_GZIP",     PH7_ZLIB_ENCODING_GZIP_Const },
	{"ZLIB_ENCODING_DEFLATE",  PH7_ZLIB_ENCODING_DEFLATE_Const },
	{"ZLIB_NO_FLUSH",          PH7_ZLIB_NO_FLUSH_Const },
	{"ZLIB_PARTIAL_FLUSH",     PH7_ZLIB_PARTIAL_FLUSH_Const },
	{"ZLIB_SYNC_FLUSH",        PH7_ZLIB_SYNC_FLUSH_Const },
	{"ZLIB_FULL_FLUSH",        PH7_ZLIB_FULL_FLUSH_Const },
	{"ZLIB_BLOCK",             PH7_ZLIB_BLOCK_Const },
	{"ZLIB_FINISH",            PH7_ZLIB_FINISH_Const },
	{"ZLIB_FILTERED",          PH7_ZLIB_FILTERED_Const },
	{"ZLIB_HUFFMAN_ONLY",      PH7_ZLIB_HUFFMAN_ONLY_Const },
	{"ZLIB_RLE",               PH7_ZLIB_RLE_Const },
	{"ZLIB_FIXED",             PH7_ZLIB_FIXED_Const },
	{"ZLIB_DEFAULT_STRATEGY",  PH7_ZLIB_DEFAULT_STRATEGY_Const },
	{"ZLIB_VERSION",           PH7_ZLIB_VERSION_Const },
	{"ZLIB_VERNUM",            PH7_ZLIB_VERNUM_Const },
	{"ZLIB_OK",                PH7_ZLIB_OK_Const },
	{"ZLIB_STREAM_END",        PH7_ZLIB_STREAM_END_Const },
	{"ZLIB_NEED_DICT",         PH7_ZLIB_NEED_DICT_Const },
	{"ZLIB_ERRNO",             PH7_ZLIB_ERRNO_Const },
	{"ZLIB_STREAM_ERROR",      PH7_ZLIB_STREAM_ERROR_Const },
	{"ZLIB_DATA_ERROR",        PH7_ZLIB_DATA_ERROR_Const },
	{"ZLIB_MEM_ERROR",         PH7_ZLIB_MEM_ERROR_Const },
	{"ZLIB_BUF_ERROR",         PH7_ZLIB_BUF_ERROR_Const },
	{"ZLIB_VERSION_ERROR",     PH7_ZLIB_VERSION_ERROR_Const },
#endif /* PH7_ENABLE_ZLIB */
#ifdef PH7_ENABLE_OPENSSL
	{"OPENSSL_VERSION_TEXT",              PH7_OPENSSL_VERSION_TEXT_Const },
	{"OPENSSL_VERSION_NUMBER",            PH7_OPENSSL_VERSION_NUMBER_Const },
	{"X509_PURPOSE_SSL_CLIENT",           PH7_X509_PURPOSE_SSL_CLIENT_Const },
	{"X509_PURPOSE_SSL_SERVER",           PH7_X509_PURPOSE_SSL_SERVER_Const },
	{"X509_PURPOSE_NS_SSL_SERVER",        PH7_X509_PURPOSE_NS_SSL_SERVER_Const },
	{"X509_PURPOSE_SMIME_SIGN",           PH7_X509_PURPOSE_SMIME_SIGN_Const },
	{"X509_PURPOSE_SMIME_ENCRYPT",        PH7_X509_PURPOSE_SMIME_ENCRYPT_Const },
	{"X509_PURPOSE_CRL_SIGN",             PH7_X509_PURPOSE_CRL_SIGN_Const },
	{"X509_PURPOSE_ANY",                  PH7_X509_PURPOSE_ANY_Const },
	{"X509_PURPOSE_OCSP_HELPER",          PH7_X509_PURPOSE_OCSP_HELPER_Const },
	{"X509_PURPOSE_TIMESTAMP_SIGN",       PH7_X509_PURPOSE_TIMESTAMP_SIGN_Const },
	{"OPENSSL_ALGO_SHA1",                 PH7_OPENSSL_ALGO_SHA1_Const },
	{"OPENSSL_ALGO_MD5",                  PH7_OPENSSL_ALGO_MD5_Const },
	{"OPENSSL_ALGO_MD4",                  PH7_OPENSSL_ALGO_MD4_Const },
	{"OPENSSL_ALGO_SHA224",               PH7_OPENSSL_ALGO_SHA224_Const },
	{"OPENSSL_ALGO_SHA256",               PH7_OPENSSL_ALGO_SHA256_Const },
	{"OPENSSL_ALGO_SHA384",               PH7_OPENSSL_ALGO_SHA384_Const },
	{"OPENSSL_ALGO_SHA512",               PH7_OPENSSL_ALGO_SHA512_Const },
	{"OPENSSL_ALGO_RMD160",               PH7_OPENSSL_ALGO_RMD160_Const },
	{"PKCS7_DETACHED",                    PH7_PKCS7_DETACHED_Const },
	{"PKCS7_TEXT",                        PH7_PKCS7_TEXT_Const },
	{"PKCS7_NOINTERN",                    PH7_PKCS7_NOINTERN_Const },
	{"PKCS7_NOVERIFY",                    PH7_PKCS7_NOVERIFY_Const },
	{"PKCS7_NOCHAIN",                     PH7_PKCS7_NOCHAIN_Const },
	{"PKCS7_NOCERTS",                     PH7_PKCS7_NOCERTS_Const },
	{"PKCS7_NOATTR",                      PH7_PKCS7_NOATTR_Const },
	{"PKCS7_BINARY",                      PH7_PKCS7_BINARY_Const },
	{"PKCS7_NOSIGS",                      PH7_PKCS7_NOSIGS_Const },
	{"PKCS7_NOOLDMIMETYPE",               PH7_PKCS7_NOOLDMIMETYPE_Const },
	{"PKCS7_NOSMIMECAP",                  PH7_PKCS7_NOSMIMECAP_Const },
	{"PKCS7_CRLFEOL",                     PH7_PKCS7_CRLFEOL_Const },
	{"PKCS7_NOCRL",                       PH7_PKCS7_NOCRL_Const },
	{"PKCS7_NO_DUAL_CONTENT",             PH7_PKCS7_NO_DUAL_CONTENT_Const },
	{"OPENSSL_CMS_DETACHED",              PH7_OPENSSL_CMS_DETACHED_Const },
	{"OPENSSL_CMS_TEXT",                  PH7_OPENSSL_CMS_TEXT_Const },
	{"OPENSSL_CMS_NOINTERN",              PH7_OPENSSL_CMS_NOINTERN_Const },
	{"OPENSSL_CMS_NOVERIFY",              PH7_OPENSSL_CMS_NOVERIFY_Const },
	{"OPENSSL_CMS_NOCERTS",               PH7_OPENSSL_CMS_NOCERTS_Const },
	{"OPENSSL_CMS_NOATTR",                PH7_OPENSSL_CMS_NOATTR_Const },
	{"OPENSSL_CMS_BINARY",                PH7_OPENSSL_CMS_BINARY_Const },
	{"OPENSSL_CMS_NOSIGS",                PH7_OPENSSL_CMS_NOSIGS_Const },
	{"OPENSSL_CMS_OLDMIMETYPE",           PH7_OPENSSL_CMS_OLDMIMETYPE_Const },
	{"OPENSSL_PKCS1_PADDING",             PH7_OPENSSL_PKCS1_PADDING_Const },
	{"OPENSSL_NO_PADDING",                PH7_OPENSSL_NO_PADDING_Const },
	{"OPENSSL_PKCS1_OAEP_PADDING",        PH7_OPENSSL_PKCS1_OAEP_PADDING_Const },
	{"OPENSSL_PKCS1_PSS_PADDING",         PH7_OPENSSL_PKCS1_PSS_PADDING_Const },
	{"OPENSSL_DEFAULT_STREAM_CIPHERS",    PH7_OPENSSL_DEFAULT_STREAM_CIPHERS_Const },
	{"OPENSSL_CIPHER_RC2_40",             PH7_OPENSSL_CIPHER_RC2_40_Const },
	{"OPENSSL_CIPHER_RC2_128",            PH7_OPENSSL_CIPHER_RC2_128_Const },
	{"OPENSSL_CIPHER_RC2_64",             PH7_OPENSSL_CIPHER_RC2_64_Const },
	{"OPENSSL_CIPHER_DES",                PH7_OPENSSL_CIPHER_DES_Const },
	{"OPENSSL_CIPHER_3DES",               PH7_OPENSSL_CIPHER_3DES_Const },
	{"OPENSSL_CIPHER_AES_128_CBC",        PH7_OPENSSL_CIPHER_AES_128_CBC_Const },
	{"OPENSSL_CIPHER_AES_192_CBC",        PH7_OPENSSL_CIPHER_AES_192_CBC_Const },
	{"OPENSSL_CIPHER_AES_256_CBC",        PH7_OPENSSL_CIPHER_AES_256_CBC_Const },
	{"OPENSSL_KEYTYPE_RSA",               PH7_OPENSSL_KEYTYPE_RSA_Const },
	{"OPENSSL_KEYTYPE_DSA",               PH7_OPENSSL_KEYTYPE_DSA_Const },
	{"OPENSSL_KEYTYPE_DH",                PH7_OPENSSL_KEYTYPE_DH_Const },
	{"OPENSSL_KEYTYPE_EC",                PH7_OPENSSL_KEYTYPE_EC_Const },
	{"OPENSSL_KEYTYPE_X25519",            PH7_OPENSSL_KEYTYPE_X25519_Const },
	{"OPENSSL_KEYTYPE_ED25519",           PH7_OPENSSL_KEYTYPE_ED25519_Const },
	{"OPENSSL_KEYTYPE_X448",              PH7_OPENSSL_KEYTYPE_X448_Const },
	{"OPENSSL_KEYTYPE_ED448",             PH7_OPENSSL_KEYTYPE_ED448_Const },
	{"OPENSSL_RAW_DATA",                  PH7_OPENSSL_RAW_DATA_Const },
	{"OPENSSL_ZERO_PADDING",              PH7_OPENSSL_ZERO_PADDING_Const },
	{"OPENSSL_DONT_ZERO_PAD_KEY",         PH7_OPENSSL_DONT_ZERO_PAD_KEY_Const },
	{"OPENSSL_TLSEXT_SERVER_NAME",        PH7_OPENSSL_TLSEXT_SERVER_NAME_Const },
	{"STREAM_CRYPTO_METHOD_SSLv2_CLIENT",              PH7_STREAM_CRYPTO_METHOD_SSLv2_CLIENT_Const },
	{"STREAM_CRYPTO_METHOD_SSLv2_SERVER",              PH7_STREAM_CRYPTO_METHOD_SSLv2_SERVER_Const },
	{"STREAM_CRYPTO_METHOD_SSLv3_CLIENT",              PH7_STREAM_CRYPTO_METHOD_SSLv3_CLIENT_Const },
	{"STREAM_CRYPTO_METHOD_SSLv3_SERVER",              PH7_STREAM_CRYPTO_METHOD_SSLv3_SERVER_Const },
	{"STREAM_CRYPTO_METHOD_SSLv23_CLIENT",             PH7_STREAM_CRYPTO_METHOD_SSLv23_CLIENT_Const },
	{"STREAM_CRYPTO_METHOD_SSLv23_SERVER",             PH7_STREAM_CRYPTO_METHOD_SSLv23_SERVER_Const },
	{"STREAM_CRYPTO_METHOD_TLS_CLIENT",                PH7_STREAM_CRYPTO_METHOD_TLS_CLIENT_Const },
	{"STREAM_CRYPTO_METHOD_TLS_SERVER",                PH7_STREAM_CRYPTO_METHOD_TLS_SERVER_Const },
	{"STREAM_CRYPTO_METHOD_TLSv1_0_CLIENT",            PH7_STREAM_CRYPTO_METHOD_TLSv1_0_CLIENT_Const },
	{"STREAM_CRYPTO_METHOD_TLSv1_0_SERVER",            PH7_STREAM_CRYPTO_METHOD_TLSv1_0_SERVER_Const },
	{"STREAM_CRYPTO_METHOD_TLSv1_1_CLIENT",            PH7_STREAM_CRYPTO_METHOD_TLSv1_1_CLIENT_Const },
	{"STREAM_CRYPTO_METHOD_TLSv1_1_SERVER",            PH7_STREAM_CRYPTO_METHOD_TLSv1_1_SERVER_Const },
	{"STREAM_CRYPTO_METHOD_TLSv1_2_CLIENT",            PH7_STREAM_CRYPTO_METHOD_TLSv1_2_CLIENT_Const },
	{"STREAM_CRYPTO_METHOD_TLSv1_2_SERVER",            PH7_STREAM_CRYPTO_METHOD_TLSv1_2_SERVER_Const },
	{"STREAM_CRYPTO_METHOD_TLSv1_3_CLIENT",            PH7_STREAM_CRYPTO_METHOD_TLSv1_3_CLIENT_Const },
	{"STREAM_CRYPTO_METHOD_TLSv1_3_SERVER",            PH7_STREAM_CRYPTO_METHOD_TLSv1_3_SERVER_Const },
	{"STREAM_CRYPTO_METHOD_ANY_CLIENT",                PH7_STREAM_CRYPTO_METHOD_ANY_CLIENT_Const },
	{"STREAM_CRYPTO_METHOD_ANY_SERVER",                PH7_STREAM_CRYPTO_METHOD_ANY_SERVER_Const },
	{"STREAM_CRYPTO_PROTO_SSLv3",                      PH7_STREAM_CRYPTO_PROTO_SSLv3_Const },
	{"STREAM_CRYPTO_PROTO_TLSv1_0",                    PH7_STREAM_CRYPTO_PROTO_TLSv1_0_Const },
	{"STREAM_CRYPTO_PROTO_TLSv1_1",                    PH7_STREAM_CRYPTO_PROTO_TLSv1_1_Const },
	{"STREAM_CRYPTO_PROTO_TLSv1_2",                    PH7_STREAM_CRYPTO_PROTO_TLSv1_2_Const },
	{"STREAM_CRYPTO_PROTO_TLSv1_3",                    PH7_STREAM_CRYPTO_PROTO_TLSv1_3_Const },
	{"OPENSSL_ENCODING_DER",              PH7_OPENSSL_ENCODING_DER_Const },
	{"OPENSSL_ENCODING_SMIME",            PH7_OPENSSL_ENCODING_SMIME_Const },
	{"OPENSSL_ENCODING_PEM",              PH7_OPENSSL_ENCODING_PEM_Const },
#endif /* PH7_ENABLE_OPENSSL */
	{"FILEINFO_NONE",          PH7_FILEINFO_NONE_Const },
	{"FILEINFO_SYMLINK",       PH7_FILEINFO_SYMLINK_Const },
	{"FILEINFO_MIME",          PH7_FILEINFO_MIME_Const },
	{"FILEINFO_MIME_TYPE",     PH7_FILEINFO_MIME_TYPE_Const },
	{"FILEINFO_MIME_ENCODING", PH7_FILEINFO_MIME_ENCODING_Const },
	{"FILEINFO_DEVICES",       PH7_FILEINFO_DEVICES_Const },
	{"FILEINFO_CONTINUE",      PH7_FILEINFO_CONTINUE_Const },
	{"FILEINFO_PRESERVE_ATIME",PH7_FILEINFO_PRESERVE_ATIME_Const },
	{"FILEINFO_RAW",           PH7_FILEINFO_RAW_Const },
	{"FILEINFO_APPLE",         PH7_FILEINFO_APPLE_Const },
	{"FILEINFO_EXTENSION",     PH7_FILEINFO_EXTENSION_Const },
	{"__CLASS__",            PH7_class_magic_Const  }
};
/*
 * Expand a built-in constant by name STRAIGHT OFF the table above, without
 * asking hConstant.
 *
 * php.ini is read before a line of the script is compiled, and PHL's equivalent
 * window -- PH7_VmApplyEngineIni -- opens between PH7_VmInit and
 * PH7_VmMakeReady, which is where the table below is installed. So a directive
 * that names a constant, and `error_reporting = E_ALL & ~E_DEPRECATED` is the
 * one everybody writes, has no hash to look it up in yet and used to read every
 * name as 0. php has the same window and answers it the same way: the constants
 * that exist for an ini value are the engine's own, which is why `M_PI` there is
 * the four letters and not 3.14159.
 *
 * Which constants php has by then was read off it directly, by asking an ini
 * value for `1|X` and watching whether X moved the answer: the E_ and PHP_
 * families resolve (E_USER_ERROR is 256, PHP_INT_SIZE is 8) and ext/standard's
 * do not (SORT_ASC and M_E are their own names, hence 0). That is the line drawn
 * here -- everything else in the table below belongs to an extension php starts
 * after it has read the file, and answering it would be answering MORE than php.
 * Returns TRUE when the name was one of those; pOut is the caller's, initialized.
 */
PH7_PRIVATE int PH7_ExpandBuiltinConstant(ph7_vm *pVm,const char *zName,sxu32 nName,ph7_value *pOut)
{
	sxu32 n;
	if( !((nName > 2 && zName[0] == 'E' && zName[1] == '_')
	   || (nName > 4 && SyMemcmp(zName,"PHP_",4) == 0)) ){
		return 0;
	}
	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){
		if( SyStrlen(aBuiltIn[n].zName) == nName
		 && SyMemcmp(aBuiltIn[n].zName,zName,nName) == 0 ){
			aBuiltIn[n].xExpand(pOut,(void *)pVm);
			return 1;
		}
	}
	return 0;
}
/*
 * Register the built-in constants defined above.
 */
PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)
{
	sxu32 n;
	/*
	 * Note that all built-in constants have access to the ph7 virtual machine
	 * that trigger the constant invocation as their private data.
	 */
	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){
		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));
	}
}
/*
 * The constants php 8.x deprecated the SYMBOL of, and the reason clause it ends
 * the notice with. Naming one raises `Constant X is deprecated since <clause>`;
 * LISTING the table does not, which is what pVm->bConstEnum is for.
 *
 * They are marked rather than raised from their own expanders because they are
 * registered in five different units -- date's, random's, the file flags',
 * curl's and dom's -- and because the export format's `<persistent, deprecated>`
 * tag has to read the same fact. The stamp runs once, after every extension has
 * installed, so a name a build does not carry is simply skipped.
 */
static const struct {
	const char *zName;
	const char *zWhy;
} aDeprecatedConst[] = {
	{ "E_STRICT",                "8.4, the error level was removed" },
	{ "DATE_RFC7231",
	  "8.5, as this format ignores the associated timezone and always uses GMT" },
	{ "FILE_TEXT",               "8.1, as the constant has no effect" },
	{ "FILE_BINARY",             "8.1, as the constant has no effect" },
	{ "MT_RAND_PHP",
	  "8.3, as it uses a biased non-standard variant of Mt19937" },
	{ "CURLOPT_BINARYTRANSFER",  "8.4, as it had no effect since 5.1.2" },
	{ "DOM_PHP_ERR",             "8.4, as it is no longer used" },
};
PH7_PRIVATE void PH7_MarkDeprecatedConstants(ph7_vm *pVm)
{
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aDeprecatedConst) ; ++n ){
		SyHashEntry *pEntry = SyHashGet(&pVm->hConstant,
			(const void *)aDeprecatedConst[n].zName,
			SyStrlen(aDeprecatedConst[n].zName));
		if( pEntry ){
			((ph7_constant *)pEntry->pUserData)->zDeprecated = aDeprecatedConst[n].zWhy;
		}
	}
}