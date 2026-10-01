/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * The PHL interpreter is a simple stand-alone PHP interpreter that allows
 * the user to enter and execute PHP files against a PH7 engine.
 * To start the phl program, just type "phl" followed by the name of the PHP file
 * to compile and execute. That is, the first argument is to the interpreter, the rest
 * are scripts arguments, press "Enter" and the PHP code will be executed.
 * If something goes wrong while processing the PHP script due to a compile-time error
 * your error output (STDOUT) should display the compile-time error messages.
 *
 * Usage example of the phl interpreter:
 *   phl hello_world.php
 * Running the interpreter with script arguments
 *    phl scripts/mp3_tag.php /usr/local/path/to/my_mp3s
 *
 * Command line options:
 *   -b: Dump PH7 byte-code instructions
 *   -h: Display this help message
 *
 * The PHL interpreter package includes more than 70 PHP scripts to test ranging from
 * simple hello world programs to XML processing, ZIP archive extracting, MP3 tag extracting,
 * UUID generation, JSON encoding/decoding, INI processing, Base32 encoding/decoding and many
 * more. These scripts are available in the scripts directory from the zip archive.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <locale.h>
#ifndef __WINNT__
#include <signal.h>   /* SIGPIPE — see main() */
#endif
#ifdef __UNIXES__
#include <unistd.h>
#endif
/* Make sure this header file is available.*/
#include "ph7.h"
#ifdef PHL_ENABLE_SERVER
#include "server.h"
#endif
#if defined(__WINNT__) && defined(PH7_DEBUG)
#define MINIDUMP_IMPLEMENTATION
#include "minidump.h"
#endif
/*
 * Display an error message and exit.
 */
static void FatalCode(const char *zMsg,int iCode)
{
	puts(zMsg);
	/* Shutdown the library */
	ph7_lib_shutdown();
	/* Exit immediately */
	exit(iCode);
}
/*
 * php-parity default: fatal engine/compile failures exit 255 (php exits 255
 * on a fatal compile error); usage and IO errors use FatalCode(msg, 1)
 * directly, mirroring php's exit 1 for bad invocations / unopenable input.
 */
static void Fatal(const char *zMsg)
{
	FatalCode(zMsg,255);
}
/*
 * Exit 255 without printing anything: used when the diagnostic has already been
 * emitted by the error consumer (a compile/parse error), which is exactly what
 * php does — it prints the parse error and nothing more.
 */
/* The engine this process built, so the silent-fatal exit can give it back:
 * ph7_lib_shutdown() releases the LIBRARY, and an engine instance owns a mutex
 * of its own. Without this the fatal path leaks it, which is invisible in
 * ordinary use (the process is exiting) and turns every leak-detecting run over
 * a corpus that spawns a failing child into a wall of reports. */
static ph7 *pFatalEngine = 0;
static void FatalSilentCode(int iCode)
{
	if( pFatalEngine ){
		ph7_release(pFatalEngine);
		pFatalEngine = 0;
	}
	ph7_lib_shutdown();
	exit(iCode);
}
static void FatalSilent(void)
{
	FatalSilentCode(255);
}
/*
 * Display the banner,a help message and exit.
 */
static void Help(void)
{
	puts("phl [-h|--help|-b|-i|-l|-v|--version|-r code|--rf name|--rc name|--re name|-d name=value|-c inifile] path/to/php_file [script args]");
#ifdef PHL_ENABLE_SERVER
	puts("phl -S host:port [-t docroot] [router.php]");
#endif
	puts("\t-b: Dump PH7 byte-code instructions");
	puts("\t-i: Display interpreter information and exit");
	puts("\t-l: Syntax-check (lint) the given file and exit");
	puts("\t-r code: Run code from command line (no tags needed)");
#ifdef PHL_ENABLE_SERVER
	puts("\t-S host:port: Start the built-in development server");
	puts("\t-t docroot: Document root for the server (default: current directory)");
#endif
	puts("\t-v, --version: Display version information and exit");
	puts("\t-h, --help: Display this message and exit");
	/* Exit immediately */
	exit(0);
}
/*
 * Display version information and exit.
 */
static void Version(void)
{
	puts("PHL " PH7_VERSION " (cli) (built " __DATE__ " " __TIME__ ")");
	puts("Copyright (c) 2011-2014 Symisc Systems, 2025 Alexandre Gomes Gaigalas");
	/* Exit immediately */
	exit(0);
}
/*
 * Display interpreter information (php -i) and exit. PHP's CLI -i is plain text
 * (the phpinfo() builtin emits HTML, suited to the web SAPI), so this prints a
 * concise curated subset on the terminal rather than reusing that builtin.
 */
static void Info(void)
{
	printf("phpinfo()\n");
	printf("PHP Version => %s\n\n", PHP_COMPAT_VERSION);
	printf("System => %s\n",
#ifdef __WINNT__
		"Windows NT"
#elif defined(__UNIXES__)
		"UNIX-Like"
#else
		"Other OS"
#endif
	);
	printf("Build Date => %s %s\n", __DATE__, __TIME__);
	printf("PHL Version => %s\n", PH7_VERSION);
	printf("PHP SAPI => cli\n");
	/* Exit immediately */
	exit(0);
}
#ifdef __WINNT__
#include <Windows.h>
#else
/* Assume UNIX */
#include <unistd.h>
#include <limits.h>
#endif
/*
 * The following define is used by the UNIX built and have
 * no particular meaning on windows.
 */
#ifndef STDOUT_FILENO
#define STDOUT_FILENO	1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO	2
#endif
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
static char zPhlBinaryPath[PATH_MAX];
/*
 * Expand callback for the PHP_BINARY constant.
 * pUserData points to the resolved binary path.
 */
static void PHL_PhpBinaryConst(ph7_value *pVal,void *pUserData)
{
	ph7_value_string(pVal,(const char *)pUserData,-1);
}
/*
 * Resolve the absolute path of the running interpreter.
 * Falls back to argv[0] verbatim (e.g. bare PATH invocation):
 * consumers spawning it again go through the shell, which re-resolves it.
 */
static const char * PHL_ResolveBinaryPath(const char *zArgv0)
{
#ifdef __WINNT__
	DWORD nLen = GetModuleFileNameA(0,zPhlBinaryPath,(DWORD)sizeof(zPhlBinaryPath));
	if( nLen > 0 && nLen < sizeof(zPhlBinaryPath) ){
		return zPhlBinaryPath;
	}
#else
	if( realpath(zArgv0,zPhlBinaryPath) != 0 ){
		return zPhlBinaryPath;
	}
#endif
	return zArgv0;
}
/*
 * VM output consumer callback.
 * Each time the virtual machine generates some outputs,the following
 * function gets called by the underlying virtual machine to consume
 * the generated output.
 * All this function does is redirecting the VM output to STDOUT.
 * This function is registered later via a call to ph7_vm_config()
 * with a configuration verb set to: PH7_VM_CONFIG_OUTPUT.
 */
static int Output_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)
{
	(void)pUserData;
#ifdef __WINNT__
	BOOL rc;
	rc = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),pOutput,(DWORD)nOutputLen,0,0);
	if( !rc ){
		/* Abort processing */
		return PH7_ABORT;
	}
#else
	ssize_t nWr;
	nWr = write(STDOUT_FILENO,pOutput,nOutputLen);
	if( nWr < 0 ){
		/* Abort processing */
		return PH7_ABORT;
	}
#endif /* __WINT__ */
	/* All done,VM output was redirected to STDOUT */
	return PH7_OK;
}
/*
 * VM diagnostics consumer (PH7_VM_CONFIG_ERR_STREAM): the log copy of a runtime
 * warning/notice/deprecation and the uncaught-exception fatal go here — STDERR —
 * so program STDOUT stays clean, matching stock CLI php.
 */
static int Error_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)
{
	(void)pUserData;
#ifdef __WINNT__
	BOOL rc;
	rc = WriteFile(GetStdHandle(STD_ERROR_HANDLE),pOutput,(DWORD)nOutputLen,0,0);
	if( !rc ){
		/* Abort processing */
		return PH7_ABORT;
	}
#else
	ssize_t nWr;
	nWr = write(STDERR_FILENO,pOutput,nOutputLen);
	if( nWr < 0 ){
		/* Abort processing */
		return PH7_ABORT;
	}
#endif /* __WINNT__ */
	/* All done, VM diagnostics were redirected to STDERR */
	return PH7_OK;
}
/*
 * Parse an unsigned-long testing knob from the environment (PHL_MAX_ALLOC /
 * PHL_MAX_INPUT / PHL_MAX_RECURSION / PHL_MAX_NATIVE_DEPTH). Returns 1 and writes
 * *pOut on a valid, strictly-positive, fully-numeric value clamped to
 * [uFloor, uCeil]; returns
 * 0 (leaving *pOut untouched) when the var is unset, empty, non-numeric, has
 * trailing garbage, or is zero — so a typo like "-1" or "abc" is ignored
 * rather than silently reinterpreted (strtoul would wrap "-1" to ULONG_MAX).
 */
static int PHL_EnvULong(const char *zName,unsigned long uFloor,unsigned long uCeil,unsigned long *pOut)
{
	const char *zVal = getenv(zName);
	char *zEnd = 0;
	unsigned long uMax;
	if( zVal == 0 || zVal[0] == 0 ){
		return 0;
	}
	/* Reject a leading sign outright: strtoul silently negates "-1" to
	 * ULONG_MAX, turning a typo into an effectively-unlimited cap. */
	if( zVal[0] == '-' || zVal[0] == '+' ){
		return 0;
	}
	errno = 0;
	uMax = strtoul(zVal,&zEnd,10);
	if( errno != 0 || zEnd == zVal || *zEnd != 0 || uMax == 0 ){
		return 0; /* non-numeric, trailing junk, overflow, or zero */
	}
	if( uMax < uFloor ){
		uMax = uFloor;
	}
	if( uMax > uCeil ){
		uMax = uCeil;
	}
	*pOut = uMax;
	return 1;
}
/*
 * php's ini scanner is not line-oriented, and a php.ini VALUE is not a line.
 * A quoted run is a scanner STATE that keeps going past the newline, so where
 * one directive's value ends -- and what line the next directive is on -- are
 * properties of the whole SOURCE:
 *
 *   . `x = 'a<NL>b'` is one raw string carrying a newline, and the rest of the
 *     file parses normally behind it. Reading a line at a time stored `'a` and
 *     called it a syntax error.
 *   . A quote with no partner runs to the end of the source and takes every
 *     directive behind it down with it -- silently when the value in front of
 *     it had already reduced (`x = (1)'`), as php's own parser does.
 *   . Only the double-quoted run moves the line counter over the newlines it
 *     ate; php's raw-string rule is a single regex match that never touches it.
 *     So a refusal below `x = "a<NL>b"` is dated one line lower than the same
 *     one below `x = 'a<NL>b'`.
 *
 * So walk the source once and hand the engine each directive with the line it
 * was written on. The value grammar itself lives in vm.c (VmIniEvalValue) and is
 * handed exactly the bytes php's scanner would have given its parser: the value
 * text with its quoted runs intact, without the comment or the newline that
 * closes the directive, and running to the end of the source when a quote never
 * closes.
 */
static const char * PHL_IniSkipEol(const char *z,const char *zEnd)
{
	while( z < zEnd && z[0] != '\n' && z[0] != '\r' ){
		z++;
	}
	return z;
}
static const char * PHL_IniEatEol(const char *z,const char *zEnd)
{
	if( z < zEnd && z[0] == '\r' ){
		z++;
	}
	if( z < zEnd && z[0] == '\n' ){
		z++;
	}
	return z;
}
/*
 * Hand one directive over. The value's TEXT is what travels: the engine runs
 * php's ini value grammar over it, which is what turns `E_ALL & ~E_NOTICE` into
 * a number, `On` into "1" and a quoted run into its literal bytes -- so
 * unquoting here would hand that grammar a constant name where php hands it
 * four letters.
 */
static void PHL_ApplyIniValue(ph7 *pEngine,const char *zName,size_t nName,
	const char *zVal,size_t nVal,const char *zFile,unsigned int nLine,int iStop)
{
	char zNameBuf[128];
	char zStack[512];
	char *zValue = zStack;   /* heap-backed past what the stack buffer holds */
	if( nName == 0 || nName >= sizeof(zNameBuf) ){
		return;
	}
	memcpy(zNameBuf,zName,nName);
	zNameBuf[nName] = 0;
	if( nVal + 1 > sizeof(zStack) ){
		/* php has no ceiling on an ini value, and a real one can be long:
		 * PHPUnit hands its child a ~700-byte redaction pattern. Truncating
		 * at a fixed width stored half a value, and cutting one in the
		 * middle of a quoted run made the whole directive a syntax error. */
		zValue = (char *)malloc(nVal + 1);
		if( zValue == 0 ){
			return;
		}
	}
	if( nVal > 0 ){
		memcpy(zValue,zVal,nVal);
	}
	zValue[nVal] = 0;
	ph7_config(pEngine,PH7_CONFIG_INI_ENTRY,zNameBuf,zValue,zFile,nLine,iStop);
	if( zValue != zStack ){
		free(zValue);
	}
}
/*
 * Hand over a `[` php's scanner never closes. It stands in the queue where a
 * directive would, carries no name, and takes the whole source down with it.
 */
static void PHL_RefuseIniSource(ph7 *pEngine,const char *zFile,unsigned int nLine,int iStop)
{
	ph7_config(pEngine,PH7_CONFIG_INI_ENTRY,"","",zFile,nLine,iStop);
}
/*
 * Hand over the text standing where php's scanner reads a directive NAME,
 * ahead of whatever is made of it. php's INITIAL is not "everything up to the
 * `=`": it has a rule for each of its bool words in front of the one that
 * reads a LABEL, and a dozen bytes of its punctuation are tokens no statement
 * of its grammar starts with -- so `on = 1` and `x]y = 1` refuse the source
 * from here down where `onx = 1` and `x.y = 1` are ordinary entries. The
 * engine owns that table, next to the value grammar that shares its bool
 * words; a second copy of it out here would drift away from it.
 */
static void PHL_ScreenIniStmt(ph7 *pEngine,const char *zStmt,size_t nStmt,
	const char *zFile,unsigned int nLine)
{
	char zStack[512];
	char *zText = zStack;
	if( nStmt + 1 > sizeof(zStack) ){
		zText = (char *)malloc(nStmt + 1);
		if( zText == 0 ){
			return;
		}
	}
	if( nStmt > 0 ){
		memcpy(zText,zStmt,nStmt);
	}
	zText[nStmt] = 0;
	ph7_config(pEngine,PH7_CONFIG_INI_ENTRY,zText,"",zFile,nLine,PH7_INI_STOP_STMT);
	if( zText != zStack ){
		free(zText);
	}
}
/*
 * Walk one whole php.ini source -- a -c file's bytes, or the buffer php's CLI
 * builds out of every -d -- and apply the directives it holds. nLine is the line
 * the first byte sits on: 1 for a file, and 6 for the -d buffer (see the caller).
 */
static void PHL_ScanIniSource(ph7 *pEngine,const char *zSrc,size_t nSrc,
	const char *zFile,unsigned int nLine)
{
	const char *z = zSrc;
	const char *zEnd = &zSrc[nSrc];
	while( z < zEnd ){
		const char *zName,*zNameEnd,*zVal,*zValEnd;
		unsigned int nDir;
		int bRunaway = 0;
		int iStop;
		while( z < zEnd && (z[0] == ' ' || z[0] == '\t') ){
			z++;
		}
		if( z >= zEnd ){
			break;
		}
		if( z[0] == '\n' || z[0] == '\r' ){
			z = PHL_IniEatEol(z,zEnd);
			nLine++;
			continue;
		}
		if( z[0] == ';' || z[0] == '#' ){
			/* comments: the newline behind them is counted above */
			z = PHL_IniSkipEol(z,zEnd);
			continue;
		}
		if( z[0] == '[' ){
			/* A section NAME is one scanner run, and the only thing that closes
			 * it is the `]`. A newline does not: php's rule has none, so a `[`
			 * with no `]` behind it on its own line runs out of source and
			 * refuses the file -- `unexpected end of file, expecting ']'`,
			 * dated where the run stopped, with every directive ABOVE it still
			 * standing and every one below it inside a header that never closed.
			 * A `]` written further down does not rescue it. Inside the name a
			 * double-quoted run hides a `]` AND crosses newlines (counting
			 * them, as it does in a value); a raw `'` run and a `${}` hide one
			 * without counting; and a backslash carries the byte behind it,
			 * the newline included. */
			unsigned int nSec = nLine;
			int iBad = PH7_INI_STOP_SECTION;
			int bOpen = 1;
			z++;
			while( z < zEnd ){
				int c = (unsigned char)z[0];
				if( c == ']' ){
					bOpen = 0;
					break;
				}
				if( c == '\n' || c == '\r' ){
					break;   /* out of name, and the file with it */
				}
				if( c == '"' ){
					const char *zQ = &z[1];
					unsigned int nEat = 0;
					while( zQ < zEnd && zQ[0] != '"' ){
						if( zQ[0] == '\\' && &zQ[1] < zEnd ){
							zQ += 2;
							continue;
						}
						if( zQ[0] == '\n' || zQ[0] == '\r' ){
							zQ = PHL_IniEatEol(zQ,zEnd);
							nEat++;
							continue;
						}
						zQ++;
					}
					nLine += nEat;
					if( zQ >= zEnd ){
						/* the quote is what ran out, and php names what it
						 * wanted instead of the `]` it never reached */
						nSec = nLine;
						iBad = PH7_INI_STOP_SECTION_STR;
						z = zEnd;
						break;
					}
					z = &zQ[1];
					continue;
				}
				if( c == '\'' ){
					const char *zQ = &z[1];
					while( zQ < zEnd && zQ[0] != '\'' ){
						zQ++;
					}
					if( zQ >= zEnd ){
						z = zEnd;
						break;   /* still `expecting ']'`, on the `[`'s own line */
					}
					z = &zQ[1];
					continue;
				}
				if( c == '$' && &z[1] < zEnd && z[1] == '{' ){
					const char *zQ = &z[2];
					while( zQ < zEnd && zQ[0] != '}' ){
						zQ++;
					}
					if( zQ >= zEnd ){
						iBad = PH7_INI_STOP_SECTION_VAR;
						z = zEnd;
						break;
					}
					z = &zQ[1];
					continue;
				}
				if( c == '\\' && &z[1] < zEnd ){
					z += 2;   /* whatever it is, a newline included, uncounted */
					continue;
				}
				z++;
			}
			if( bOpen ){
				PHL_RefuseIniSource(pEngine,zFile,nSec,iBad);
				return;
			}
			/* php's rule for the `]` that closes a header eats the blanks
			 * behind it, eats a newline when one is there, and counts a line
			 * either way. So a directive written on the header's own line is
			 * read like any other -- `[s] precision=9` sets precision -- and
			 * a refusal below a header that did not end its own line is dated
			 * one line lower than it was typed. */
			z++;
			while( z < zEnd && (z[0] == ' ' || z[0] == '\t') ){
				z++;
			}
			if( z < zEnd && (z[0] == '\n' || z[0] == '\r') ){
				z = PHL_IniEatEol(z,zEnd);
			}
			nLine++;
			continue;
		}
		nDir = nLine;
		zName = z;
		while( z < zEnd && z[0] != '=' && z[0] != '\n' && z[0] != '\r' ){
			z++;
		}
		zNameEnd = z;
		while( zNameEnd > zName && (zNameEnd[-1] == ' ' || zNameEnd[-1] == '\t') ){
			zNameEnd--;
		}
		/* An `=` with nothing in front of it leaves the run empty, and php's
		 * scanner has a token there all the same. */
		PHL_ScreenIniStmt(pEngine,zName,
			zNameEnd > zName ? (size_t)(zNameEnd - zName) : (size_t)(z < zEnd ? 1 : 0),
			zFile,nDir);
		if( z >= zEnd || z[0] != '=' ){
			/* php's php.ini callback ignores a statement that carries no
			 * value, so a bare name neither defines the entry nor sets it to
			 * "1" -- `parse_ini_string("precision")` is the empty array on
			 * both sides of the fence for the same reason. */
			continue;
		}
		z++;   /* past the '=' */
		zVal = z;
		while( z < zEnd ){
			int c = (unsigned char)z[0];
			if( c == '\n' || c == '\r' || c == ';' ){
				break;
			}
			if( c == '"' ){
				const char *zQ = &z[1];
				unsigned int nEat = 0;
				while( zQ < zEnd && zQ[0] != '"' ){
					if( zQ[0] == '\\' && &zQ[1] < zEnd ){
						/* a tool that has to put a quote in a value writes `\"` */
						zQ += 2;
						continue;
					}
					if( zQ[0] == '\n' || zQ[0] == '\r' ){
						zQ = PHL_IniEatEol(zQ,zEnd);
						nEat++;
						continue;
					}
					zQ++;
				}
				nLine += nEat;
				if( zQ >= zEnd ){
					bRunaway = 1;
					z = zEnd;
					break;
				}
				z = &zQ[1];
				continue;
			}
			if( c == '\'' ){
				const char *zQ = &z[1];
				while( zQ < zEnd && zQ[0] != '\'' ){
					zQ++;
				}
				if( zQ >= zEnd ){
					bRunaway = 1;
					z = zEnd;
					break;
				}
				z = &zQ[1];   /* no line counting: the raw rule is one match */
				continue;
			}
			if( c == '$' ){
				/* php's VALUE_CHARS unit `("$"[^{])` carries the byte behind
				 * the `$` whatever that byte is -- the NEWLINE included -- and
				 * one more when it is a backslash. So a value whose last byte
				 * is `$` does not end at its own line: it eats the newline and
				 * runs on into the text below, which is how `x=a$` reaches the
				 * next directive's `=` and refuses the file there. The line
				 * counter does NOT move for it: only php's own NEWLINE rule
				 * counts, and this newline went to the value scanner instead,
				 * so a refusal below a run-on is dated one line short of where
				 * it was typed. A `$` with nothing behind it matches nothing at
				 * all, and the value stops in front of it. */
				if( &z[1] >= zEnd ){
					break;
				}
				if( z[1] == '{' ){
					z++;   /* `${NAME}` is its own production, not this unit */
					continue;
				}
				z += z[1] == '\\' && &z[2] < zEnd ? 3 : 2;
				continue;
			}
			z++;
		}
		zValEnd = z;
		iStop = PH7_INI_STOP_EOF;
		if( !bRunaway ){
			/* the `;` comment, and the newline that closes the directive, are
			 * the scanner's own tokens rather than part of the value */
			int bComment = z < zEnd && z[0] == ';';
			z = PHL_IniSkipEol(z,zEnd);
			/* Which of the two php refuses a value that merely ran out under.
			 * Its NEWLINE rule counts the line it eats, so a directive closed by
			 * one is dated a line low; a source that ends without one has no
			 * newline to give, and a comment left hanging there matches nothing
			 * at all -- what the parser gets then is the end of the input. */
			iStop = z < zEnd ? PH7_INI_STOP_EOL
				: bComment ? PH7_INI_STOP_COMMENT : PH7_INI_STOP_EOF;
		}
		PHL_ApplyIniValue(pEngine,zName,(size_t)(zNameEnd - zName),
			zVal,(size_t)(zValEnd - zVal),zFile,nDir,iStop);
	}
}
/*
 * One -d, as php's ini builder writes it into that buffer: `name=value` and a
 * newline, with the value wrapped in double quotes whenever its first byte is
 * not alphanumeric and not already a quote -- php's own workaround, applied to
 * the argv text before any trimming, and what makes `-d 'x=(E_ALL) ^ E_NOTICE'`
 * and `-d 'x=~~2'` store their own text where the same lines in a -c file are
 * expressions. A bare `-d name` is written `name=1`. Returns the bytes written.
 */
static size_t PHL_IniDefineLine(const char *zPair,char *zOut)
{
	const char *zEq = strchr(zPair,'=');
	size_t n;
	if( zEq == 0 ){
		n = strlen(zPair);
		memcpy(zOut,zPair,n);
		memcpy(&zOut[n],"=1\n",sizeof("=1\n")-1);
		return n + sizeof("=1\n") - 1;
	}
	{
		const char *zV = &zEq[1];
		size_t nHead = (size_t)(zEq - zPair) + 1;   /* the name and its '=' */
		size_t nV = strlen(zV);
		int bQuote = zV[0] != 0 && zV[0] != '"' && zV[0] != '\''
		          && !((zV[0] >= '0' && zV[0] <= '9')
		            || (zV[0] >= 'a' && zV[0] <= 'z')
		            || (zV[0] >= 'A' && zV[0] <= 'Z'));
		memcpy(zOut,zPair,nHead);
		n = nHead;
		if( bQuote ){
			zOut[n++] = '"';
		}
		if( nV > 0 ){
			memcpy(&zOut[n],zV,nV);
			n += nV;
		}
		if( bQuote ){
			zOut[n++] = '"';
		}
		zOut[n++] = '\n';
		return n;
	}
}
/*
 * Load php.ini directives from a -c file. Read whole: a value may span lines
 * (see PHL_ScanIniSource), so no line of it can be read on its own.
 */
static void PHL_LoadIniFile(ph7 *pEngine,const char *zPath)
{
	char *zSrc;
	long nSize;
	size_t nRead;
	FILE *pFile = fopen(zPath,"rb");
	if( pFile == 0 ){
		fprintf(stderr,"Could not open php.ini file: %s\n",zPath);
		return;
	}
	if( fseek(pFile,0,SEEK_END) != 0 || (nSize = ftell(pFile)) < 0
	 || fseek(pFile,0,SEEK_SET) != 0 ){
		fclose(pFile);
		return;
	}
	zSrc = (char *)malloc((size_t)nSize + 1);
	if( zSrc == 0 ){
		fclose(pFile);
		return;
	}
	nRead = fread(zSrc,1,(size_t)nSize,pFile);
	fclose(pFile);
	zSrc[nRead] = 0;
	PHL_ScanIniSource(pEngine,zSrc,nRead,zPath,1);
	free(zSrc);
}
/*
 * Return TRUE when standard input is a pipe/redirect rather than an interactive
 * terminal. php reads the script from stdin in exactly that case; a terminal
 * would block waiting for input, so there we keep the usage error instead.
 */
static int PHL_StdinIsPipe(void)
{
#ifdef __UNIXES__
	return !isatty(0);
#else
	return 0;
#endif
}
/*
 * Slurp all of standard input into a heap buffer (NUL-terminated). Returns the
 * buffer (caller owns it, though the process exits shortly after) and writes the
 * byte length to *pnLen, or NULL on allocation failure.
 */
static char * PHL_SlurpStdin(int *pnLen)
{
	size_t nCap = 8192, nUsed = 0;
	char *zBuf = (char *)malloc(nCap);
	if( zBuf == 0 ){ return 0; }
	for(;;){
		size_t nRd;
		if( nUsed + 4096 + 1 > nCap ){
			char *zNew;
			nCap *= 2;
			zNew = (char *)realloc(zBuf, nCap);
			if( zNew == 0 ){ free(zBuf); return 0; }
			zBuf = zNew;
		}
		nRd = fread(zBuf + nUsed, 1, 4096, stdin);
		nUsed += nRd;
		if( nRd < 4096 ){ break; }
	}
	zBuf[nUsed] = 0;
	*pnLen = (int)nUsed;
	return zBuf;
}
/*
 * Main program: Compile and execute the PHP file.
 */
int main(int argc,char **argv)
{
	ph7 *pEngine; /* PH7 engine */
	ph7_vm *pVm;  /* Compiled PHP program */
	int dump_vm = 0;    /* Dump VM instructions if TRUE */
	int run_code = 0;    /* Run inline code if TRUE */
	int lint_mode = 0;   /* Syntax-check only (-l) if TRUE */
	const char *zRunCode = 0; /* Inline code string */
	int stdin_code = 0;       /* Execute a script read from stdin if TRUE */
	char *zStdinCode = 0;     /* Script slurped from stdin */
	int nStdinCode = 0;       /* Length of the stdin script */
	int dash_dash = 0;        /* Saw `--`: read from stdin, rest are script args */
#ifdef PHL_ENABLE_SERVER
	int server_mode = 0;        /* Start built-in server if TRUE */
	const char *zServerAddr = 0; /* host:port string */
	const char *zDocRoot = ".";  /* Document root */
#endif
	int n;              /* Script arguments */
	int rc;
	const char *azIniDefine[64]; /* -d name=value directives, in order */
	int nIniDefine = 0;
	const char *zIniFile = 0;    /* -c php.ini path */
	/* php's CLI ignores SIGPIPE for the whole process, and this is PHL's CLI: a
	 * write to a pipe or socket whose reader is gone then answers EPIPE instead of
	 * KILLING the interpreter, which is the answer every php program is written
	 * against. The disposition is inherited across fork AND exec, so it is also
	 * what a proc_open() child runs with -- `cat` writing into a pipe the script
	 * has closed exits 1 with a write error under php, where a default SIGPIPE
	 * kills it and proc_close() reports 141. The networking subsystem used to set
	 * this lazily at the first socket, so a program that opened none never had it. */
#if defined(SIGPIPE) && defined(SIG_IGN)
	signal(SIGPIPE,SIG_IGN);
#endif
	/* php's own module startup pins LC_CTYPE to C.UTF-8 and leaves every other
	 * category at C, whatever the environment says -- so `setlocale(LC_CTYPE,'0')`
	 * answers "C.UTF-8" under php on a box with the locale and "C" on one without,
	 * and never the LANG the shell exported. That is observable on its own, and it
	 * also decides the encoding ext/gettext hands an answer back in when nothing
	 * called bind_textdomain_codeset(). The engine LIBRARY does not touch the
	 * process locale; this is the CLI, which is PHL's SAPI. */
	if( setlocale(LC_CTYPE,"C.UTF-8") == 0 ){
		setlocale(LC_CTYPE,"C");
	}
	/* Process interpreter arguments first*/
	for(n = 1 ; n < argc ; ++n ){
		int c;
		if( argv[n][0] != '-' ){
			/* No more interpreter arguments */
			break;
		}
		/* Check for long options */
		if( argv[n][1] == '-' ){
			if( argv[n][2] == 0 ){
				/* php CLI parity: a bare `--` ends interpreter options; the
				 * script is read from stdin and everything after `--` becomes
				 * the script's own arguments ($argv[1..]). */
				dash_dash = 1;
				n++;
				break;
			}
			if( strcmp(argv[n], "--version") == 0 ){
				Version();
			}else if( strcmp(argv[n], "--help") == 0 ){
				Help();
			}else if( strcmp(argv[n], "--rf") == 0 || strcmp(argv[n], "--rc") == 0 ){
				/* php CLI parity: `--rf <function>` / `--rc <class>` print the
				 * Reflection export (the __toString machinery is byte-exact vs
				 * php) and exit 1 with `Exception: <message>` when the target
				 * does not exist. Implemented as an inline snippet riding the
				 * -r code path; the NAME is charset-validated (identifier +
				 * namespace separators) so it embeds safely in the snippet. */
				static char zReflCode[768];
				const char *zWhat = (argv[n][3] == 'f') ? "ReflectionFunction" : "ReflectionClass";
				const char *zName;
				const char *zChk;
				if( n + 1 >= argc ){
					FatalCode("Missing name argument for --rf/--rc",1);
				}
				zName = argv[++n];
				for( zChk = zName ; *zChk ; zChk++ ){
					char ch = *zChk;
					if( !(ch == '_' || ch == '\\'
					   || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
					   || (ch >= '0' && ch <= '9')) ){
						FatalCode("Invalid name for --rf/--rc",1);
					}
				}
				if( strlen(zName) > 250 ){
					FatalCode("Name too long for --rf/--rc",1);
				}
				{
					/* Double the namespace separators: inside the snippet's
					 * double-quoted string a lone backslash could form an
					 * escape sequence ("App\name" -> newline). */
					static char zEsc[512];
					char *pOut = zEsc;
					for( zChk = zName ; *zChk ; zChk++ ){
						if( *zChk == '\\' ){
							*pOut++ = '\\';
						}
						*pOut++ = *zChk;
					}
					*pOut = 0;
					snprintf(zReflCode,sizeof(zReflCode),
						"try { echo new %s(\"%s\"), \"\\n\"; } catch (Throwable $e) { echo \"Exception: \", $e->getMessage(), \"\\n\"; exit(1); }",
						zWhat,zEsc);
				}
				zRunCode = zReflCode;
				run_code = 1;
			}else if( strcmp(argv[n], "--re") == 0 ){
				/* php CLI parity: `--re <extension>` prints the module's whole
				 * Reflection export. An extension NAME is not an identifier --
				 * php has `Zend OPcache` and `pdo_sqlite` -- so the charset is
				 * --rz's, and the refusal is ReflectionExtension's own. */
				static char zExtCode[768];
				const char *zName;
				const char *zChk;
				if( n + 1 >= argc ){
					FatalCode("Missing name argument for --re",1);
				}
				zName = argv[++n];
				for( zChk = zName ; *zChk ; zChk++ ){
					char ch = *zChk;
					if( !(ch == '_' || ch == ' ' || ch == '.' || ch == '-'
					   || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
					   || (ch >= '0' && ch <= '9')) ){
						FatalCode("Invalid name for --re",1);
					}
				}
				if( strlen(zName) > 250 ){
					FatalCode("Name too long for --re",1);
				}
				snprintf(zExtCode,sizeof(zExtCode),
					"try { echo new ReflectionExtension(\"%s\"), \"\n\"; }"
					" catch (Throwable $e) { echo \"Exception: \", $e->getMessage(), \"\n\"; exit(1); }",
					zName);
				zRunCode = zExtCode;
				run_code = 1;
			}else if( strcmp(argv[n], "--rz") == 0 ){
				/* php CLI parity: `--rz <name>` reflects a ZEND extension. PHL
				 * loads none, so every name is the refusal php prints for one
				 * it does not have -- which is ReflectionZendExtension's own
				 * constructor, reached the way --rf/--rc reach theirs. */
				static char zZendCode[768];
				const char *zName;
				const char *zChk;
				if( n + 1 >= argc ){
					FatalCode("Missing name argument for --rz",1);
				}
				zName = argv[++n];
				for( zChk = zName ; *zChk ; zChk++ ){
					char ch = *zChk;
					if( !(ch == '_' || ch == ' ' || ch == '.' || ch == '-'
					   || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
					   || (ch >= '0' && ch <= '9')) ){
						FatalCode("Invalid name for --rz",1);
					}
				}
				if( strlen(zName) > 250 ){
					FatalCode("Name too long for --rz",1);
				}
				snprintf(zZendCode,sizeof(zZendCode),
					"try { echo new ReflectionZendExtension(\"%s\"), \"\n\"; }"
					" catch (Throwable $e) { echo \"Exception: \", $e->getMessage(), \"\n\"; exit(1); }",
					zName);
				zRunCode = zZendCode;
				run_code = 1;
			}else{
				/* Unknown long option */
				Help();
			}
			continue;
		}
		c = argv[n][1];
		if( c == 'b' ){
			/* Dump byte-code instructions */
			dump_vm = 1;
		}else if( c == 'l' ){
			/* Syntax-check only (lint) the file argument that follows */
			lint_mode = 1;
		}else if( c == 'i' ){
			/* Display interpreter information and exit */
			Info();
		}else if( c == 'm' ){
			/* php CLI parity: `-m` lists the loaded modules, php's two sections
			 * and its case-insensitive order. PHL loads no Zend extension, so
			 * that section is always the header and nothing under it -- which
			 * is php's own shape for a build with none. Rides the -r code path
			 * the way --rf/--rc do. */
			zRunCode =
				"$m = get_loaded_extensions();"
				"usort($m, 'strcasecmp');"
				"echo \"[PHP Modules]\n\";"
				"foreach ($m as $x) { echo $x, \"\n\"; }"
				"echo \"\n[Zend Modules]\n\";"
				"foreach (get_loaded_extensions(true) as $x) { echo $x, \"\n\"; }"
				"echo \"\n\";";
			run_code = 1;
		}else if( c == 'f' ){
			/* php CLI parity: `-f <file>` explicitly names the script to run.
			 * The path follows as the next argument, which the positional
			 * file handling below already consumes, so treat -f as a no-op. */
			continue;
		}else if( c == 'r' ){
			/* Run inline PHP code from next argument (php -r style) */
			if( n + 1 >= argc ){
				/* Missing code argument */
				FatalCode("Missing code argument for -r",1);
			}
			zRunCode = argv[++n];
			run_code = 1;
		}else if( c == 'S' ){
			/* Start built-in development server */
#ifdef PHL_ENABLE_SERVER
			if( n + 1 >= argc ){
				FatalCode("Missing host:port argument for -S",1);
			}
			zServerAddr = argv[++n];
			server_mode = 1;
#else
			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");
#endif
		}else if( c == 't' ){
			/* Set document root for the server */
#ifdef PHL_ENABLE_SERVER
			if( n + 1 >= argc ){
				FatalCode("Missing docroot argument for -t",1);
			}
			zDocRoot = argv[++n];
#else
			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");
#endif
		}else if( c == 'v' ){
			/* Display version */
			Version();
		}else if( c == 'd' ){
			/* php CLI parity: -d name=value defines a php.ini entry
			 * (repeatable; applied to the VM after compile, in order). */
			if( n + 1 >= argc ){
				FatalCode("Missing name=value argument for -d",1);
			}
			if( nIniDefine < (int)(sizeof(azIniDefine)/sizeof(azIniDefine[0])) ){
				azIniDefine[nIniDefine++] = argv[++n];
			}else{
				FatalCode("Too many -d directives",1);
			}
		}else if( c == 'c' ){
			/* php CLI parity: -c file loads php.ini directives from a file
			 * (name=value lines; [sections] and ;/# comments ignored). */
			if( n + 1 >= argc ){
				FatalCode("Missing file argument for -c",1);
			}
			zIniFile = argv[++n];
		}else{
			/* Display a help message and exit */
			Help();
		}
	}
#ifdef PHL_ENABLE_SERVER
	if( server_mode ){
		/* Parse host:port from zServerAddr */
		char zHost[256];
		int iPort = 0;
		const char *zColon;
		const char *zRouter = 0;
		zColon = strrchr(zServerAddr, ':');
		if( zColon == 0 ){
			FatalCode("Invalid address format. Use host:port (e.g., localhost:8080)",1);
		}
		{
			int nHostLen = (int)(zColon - zServerAddr);
			if( nHostLen >= (int)sizeof(zHost) ) nHostLen = (int)sizeof(zHost) - 1;
			memcpy(zHost, zServerAddr, nHostLen);
			zHost[nHostLen] = 0;
		}
		iPort = atoi(zColon + 1);
		if( iPort <= 0 || iPort > 65535 ){
			FatalCode("Invalid port number",1);
		}
		/* Check for optional router script */
		if( n < argc ){
			zRouter = argv[n];
		}
		return phl_serve(zHost, iPort, zDocRoot, zRouter, PHL_ResolveBinaryPath(argv[0]));
	}
#endif
	if( (n >= argc || dash_dash) && !run_code ){
		/* No file and no -r: php reads the script from stdin. `--` forces this
		 * (rest are script args); otherwise only when stdin is a pipe/redirect
		 * (an interactive terminal would just block). */
		if( dash_dash || PHL_StdinIsPipe() ){
			zStdinCode = PHL_SlurpStdin(&nStdinCode);
			if( zStdinCode == 0 ){
				FatalCode("IO error while reading standard input",1);
			}
			stdin_code = 1;
		}else{
			puts("Missing PHP file to compile");
			Help();
		}
	}

#if defined(__WINNT__) && defined(PH7_DEBUG)
	/* Install an unhandled exception minidump handler for Windows debug builds */
	CreateMiniDumpOnUnHandledException();
#endif
	/* Allocate a new PH7 engine instance */
	rc = ph7_init(&pEngine);
	pFatalEngine = pEngine;
	if( rc != PH7_OK ){
		/*
		 * If the supplied memory subsystem is so sick that we are unable
		 * to allocate a tiny chunk of memory,there is no much we can do here.
		 */
		Fatal("Error while allocating a new PH7 engine instance");
	}
	/* Compile-time diagnostics go to STDERR, where php's log copy goes. This used
	 * to be Output_Consumer, i.e. STDOUT -- so `phl -l bad.php` printed the parse
	 * error into program output, and anything capturing a script's stdout read
	 * php's stderr text as data. It is a FALLBACK channel: once the VM's own
	 * streams are wired below, the engine routes each copy to the right one, and
	 * this only serves the window in which the main script's own compile creates
	 * the VM. */
	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,
		Error_Consumer, /* Compile-time diagnostic consumer (STDERR) */
		0 /* NULL: Callback Private data */
		);
	/* ...and the program-output stream the DISPLAY copy of that same diagnostic
	 * takes, which php puts on STDOUT. Also a fallback: the VM's own output
	 * consumer is installed below, and does not exist while the main script is
	 * being compiled. */
	ph7_config(pEngine,PH7_CONFIG_OUTPUT,
		Output_Consumer, /* Compile-time DISPLAY copy consumer (STDOUT) */
		0 /* NULL: Callback Private data */
		);
	/* Report script run-time errors (now default behavior), then the php.ini
	 * directives, and all of it BEFORE anything is compiled -- which is where php
	 * reads php.ini. These used to be applied to the finished VM, i.e. after the
	 * main script's own compile had already raised every diagnostic it was going
	 * to: `-d display_errors=1` never moved a parse error onto stdout, and
	 * `-d log_errors=0` never took one off stderr, whatever php did. The -c file
	 * comes first and -d overrides it in CLI order (php's precedence), and the
	 * whole queue lands after the error-report default so `-d error_reporting=0`
	 * can still lower it. */
	ph7_config(pEngine,PH7_CONFIG_ERR_REPORT);
	if( zIniFile ){
		PHL_LoadIniFile(pEngine,zIniFile);
	}
	if( nIniDefine > 0 ){
		/* php's CLI SAPI joins every -d into ONE buffer for zend_parse_ini_string(),
		 * prefixed by five lines of its own hardcoded startup ini (php_cli.c's
		 * HARDCODED_INI: html_errors, implicit_flush, output_buffering,
		 * max_execution_time, max_input_time) -- so the first -d is on line 6 and
		 * a directive's line, in the "on line N" a refusal warns about, is 5 plus
		 * its 1-based position among every -d php was given.
		 *
		 * Join them the same way rather than applying each on its own: they are
		 * ONE scanner input, so a quoted run one -d opens keeps going into the
		 * next one's text exactly as it would inside a file. `-d "x='a" -d "y=b'"`
		 * is a single raw string spanning both, and an unterminated `"` in the
		 * first swallows every -d behind it. */
		size_t nCap = 0, nUsed = 0;
		char *zBuf;
		int i;
		for( i = 0 ; i < nIniDefine ; i++ ){
			nCap += strlen(azIniDefine[i]) + sizeof("\"\"=1\n");
		}
		zBuf = (char *)malloc(nCap);
		if( zBuf ){
			for( i = 0 ; i < nIniDefine ; i++ ){
				nUsed += PHL_IniDefineLine(azIniDefine[i],&zBuf[nUsed]);
			}
			PHL_ScanIniSource(pEngine,zBuf,nUsed,"Unknown",6);
			free(zBuf);
		}
	}
	/* Optional per-allocation memory cap (PHL_MAX_ALLOC=bytes). Used to
	 * deterministically exercise out-of-memory paths (see tests/ph7/003-stress).
	 * Clamp to a floor above the pool bucket size (SXMEM_POOL_MAXALLOC, 32 KB)
	 * so the engine can still start; VMs inherit it at creation. */
	{
		unsigned long uMax;
		/* floor: keep above the pool bucket size; clamp: nMaxRequest is 32-bit */
		if( PHL_EnvULong("PHL_MAX_ALLOC",65536UL,0xFFFFFFFFUL,&uMax) ){
			ph7_config(pEngine,PH7_CONFIG_MAX_ALLOC,(unsigned int)uMax);
		}
	}
	/* Optional per-input byte cap (PHL_MAX_INPUT=bytes). Used to exercise the
	 * input-size rejection path at a manageable scale (see tests/ph7/003-stress). */
	{
		unsigned long uMax;
		if( PHL_EnvULong("PHL_MAX_INPUT",1UL,0xFFFFFFFFUL,&uMax) ){
			ph7_config(pEngine,PH7_CONFIG_MAX_INPUT,(unsigned int)uMax);
		}
	}
	/* Syntax-check only mode (-l): compile the target file, print PHP's summary
	 * line and exit without executing. The error consumer installed above
	 * already prints any parse error; ph7_compile_file leaves *pVm NULL on a
	 * compile/IO error, so only a successful compile owns a VM to release. */
	if( lint_mode ){
		const char *zFile;
		if( n >= argc ){
			/* No file argument (e.g. `-l` alone, or `-l` mixed with `-r`). */
			ph7_release(pEngine);
			pFatalEngine = 0;
			puts("No input file specified");
			return 255;
		}
		zFile = argv[n];
		rc = ph7_compile_file(pEngine,zFile,&pVm,PH7_SYNTAX_CHECK);
		if( rc == PH7_OK ){
			printf("No syntax errors detected in %s\n",zFile);
			ph7_vm_release(pVm);
		}else if( rc == PH7_IO_ERR ){
			/* php says this on STDERR, like the run path above */
			fprintf(stderr,"Could not open input file: %s\n",zFile);
		}else{
			printf("Errors parsing %s\n",zFile);
		}
		ph7_release(pEngine);
		pFatalEngine = 0;
		/* php's two exit codes are not one: a file it cannot OPEN is 1 (a bad
		 * invocation), a file that does not PARSE is 255. */
		if( rc == PH7_OK ){
			return 0;
		}
		return (rc == PH7_IO_ERR) ? 1 : 255;
	}
	/* Now,it's time to compile our PHP file */
	if( run_code ){
		/* Compile inline PHP code string (PHP only - no tags needed) */
		rc = ph7_compile_v2(
			pEngine, /* PH7 Engine */
			zRunCode, /* Source code */
			-1,       /* Let API compute length */
			&pVm,     /* OUT: Compiled PHP program */
			PH7_PHP_ONLY /* Inline PHP, no tags expected */
			);
		if( rc != PH7_OK ){ /* Compile error */
			if( rc == PH7_VM_ERR ){
				Fatal("VM initialization error");
			}else{
				/* Compile-time error. The diagnostic has already been printed by the
				 * error consumer; php adds nothing else, it just exits 255. */
				FatalSilent();
			}
		}
	}else if( stdin_code ){
		/* Script read from stdin: compile it like a file (PHP tags expected). */
		rc = ph7_compile_v2(
			pEngine,     /* PH7 Engine */
			zStdinCode,  /* Source code slurped from stdin */
			nStdinCode,  /* Its byte length */
			&pVm,        /* OUT: Compiled PHP program */
			0            /* IN: tag mode, like a file */
			);
		if( rc != PH7_OK ){ /* Compile error */
			if( rc == PH7_VM_ERR ){
				Fatal("VM initialization error");
			}else{
				FatalSilent();
			}
		}
	}else{
		rc = ph7_compile_file(
			pEngine, /* PH7 Engine */
			argv[n], /* Path to the PHP file to compile */
			&pVm,    /* OUT: Compiled PHP program */
			0        /* IN: Compile flags */
			);
		if( rc != PH7_OK ){ /* Compile error */
			if( rc == PH7_IO_ERR ){
				/* php names the file it could not open, and says so on STDERR --
				 * this answered a generic "IO error while opening the target file"
				 * on stdout, which a script capturing program output then read as
				 * part of the answer. */
				fprintf(stderr,"Could not open input file: %s\n",argv[n]);
				FatalSilentCode(1);
			}else if( rc == PH7_VM_ERR ){
				Fatal("VM initialization error");
			}else{
				/* Compile-time error. The diagnostic has already been printed by the
				 * error consumer; php prints nothing further and exits 255. */
				FatalSilent();
			}
		}
	}
	/*
	 * Now we have our script compiled,it's time to configure our VM.
	 * We will install the VM output consumer callback defined above
	 * so that we can consume the VM output and redirect it to STDOUT.
	 */
	rc = ph7_vm_config(pVm,
		PH7_VM_CONFIG_OUTPUT,
		Output_Consumer,    /* Output Consumer callback */
		0                   /* Callback private data */
		);
	if( rc != PH7_OK ){
		Fatal("Error while installing the VM output consumer callback");
	}
	/* Diagnostics stream: route the log copy of runtime warnings/notices and the
	 * uncaught-exception fatal to STDERR (gated by log_errors), so program STDOUT
	 * stays clean like stock CLI php. */
	rc = ph7_vm_config(pVm,
		PH7_VM_CONFIG_ERR_STREAM,
		Error_Consumer,     /* Diagnostics (STDERR) consumer callback */
		0                   /* Callback private data */
		);
	if( rc != PH7_OK ){
		Fatal("Error while installing the VM diagnostics consumer callback");
	}
	/* Optional recursion caps via the environment (like PHL_MAX_ALLOC). The host
	 * defaults are PHP-parity — PHP call depth is UNBOUNDED (heap-bound) and only
	 * the native VmByteCodeExec nesting is capped — so these knobs are for tests
	 * and embedders that want a tighter bound, not to raise a low default.
	 *   PHL_MAX_RECURSION   -> PH7_VM_CONFIG_RECURSION_DEPTH (PHP call depth; any
	 *                          positive value is a cap, PHL_EnvULong rejects 0)
	 *   PHL_MAX_NATIVE_DEPTH -> PH7_VM_CONFIG_NATIVE_DEPTH   (native nesting;
	 *                          floor 2) */
	{
		unsigned long uMax;
		if( PHL_EnvULong("PHL_MAX_RECURSION",1UL,0x7FFFFFFFUL,&uMax) ){
			ph7_vm_config(pVm,PH7_VM_CONFIG_RECURSION_DEPTH,(int)uMax);
		}
		if( PHL_EnvULong("PHL_MAX_NATIVE_DEPTH",2UL,0x7FFFFFFFUL,&uMax) ){
			ph7_vm_config(pVm,PH7_VM_CONFIG_NATIVE_DEPTH,(int)uMax);
		}
	}
	/* Define PHP_BINARY: absolute path of this interpreter */
	ph7_create_constant(pVm,"PHP_BINARY",PHL_PhpBinaryConst,
		(void *)PHL_ResolveBinaryPath(argv[0]));
	/* Register the script arguments as $argv[] plus the matching $argc count and
	 * the CLI $_SERVER entries, matching PHP: $argv[0] is the script path (file
	 * mode) or the literal "Standard input code" (-r mode), followed by the
	 * script's own arguments.
	 */
	{
		const char *zScriptName = (run_code || stdin_code) ? "Standard input code" : argv[n];
		int argv_count = 0;
		ph7_value *pArgc;
		/* Count only the entries actually inserted, so $argc can never disagree
		 * with count($argv) if a registration fails. */
		if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,zScriptName) == PH7_OK ){
			argv_count++;
		}
		/* The script's own arguments follow: in file mode argv[n] is the script
		 * (registered above), so they start at n+1; in -r mode they start at n. */
		for( n = (run_code || stdin_code) ? n : n + 1; n < argc ; ++n ){
			if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,argv[n]) == PH7_OK ){
				argv_count++;
			}
		}
		/* $argc: a plain integer global equal to count($argv). */
		pArgc = ph7_new_scalar(pVm);
		if( pArgc ){
			ph7_value_int(pArgc,argv_count);
			ph7_vm_config(pVm,PH7_VM_CONFIG_CREATE_VAR,"argc",pArgc);
			ph7_release_value(pVm,pArgc);
		}
		/* Mirror $argv/$argc into $_SERVER['argv']/$_SERVER['argc'] (php CLI). */
		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ARGV);
		/* $_SERVER entries frameworks read at CLI bootstrap. SCRIPT_FILENAME is
		 * already set to the script path by PH7_HashmapCreateSuper. */
		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"SCRIPT_NAME",zScriptName,-1);
		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PHP_SELF",zScriptName,-1);
		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"DOCUMENT_ROOT","",0);
		{
			char zTime[32];
			snprintf(zTime,sizeof(zTime),"%ld",(long)time(0));
			ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"REQUEST_TIME",zTime,-1);
		}
#ifndef __WINNT__
		{
			char zCwd[PATH_MAX];
			if( getcwd(zCwd,sizeof(zCwd)) ){
				ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PWD",zCwd,-1);
			}
		}
#endif
	}
	if( dump_vm ){
		/* Dump PH7 byte-code instructions */
		ph7_vm_dump_v2(pVm,
			Output_Consumer, /* Dump consumer callback */
			0
			);
	}
	/*
	 * And finally, execute our program. Note that your output (STDOUT in our case)
	 * should display the result.
	 */
	{
		int iExitStatus = 0;
		ph7_vm_exec(pVm,&iExitStatus);
		/* All done, cleanup the mess left behind.
		*/
		ph7_vm_release(pVm);
		ph7_release(pEngine);
		pFatalEngine = 0;
		/* The stdin slurp outlives compilation (the compiler keeps pointers
		 * into the source text), so it is freed only here, after the VM. */
		if( zStdinCode ){
			free(zStdinCode);
		}
		/* Propagate the script exit status (set via exit()/die()) */
		return iExitStatus;
	}
}
