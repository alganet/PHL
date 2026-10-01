# src/phl/phl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 381/490 lines (77.76%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `/*` |
|     - |    7 | ` * The PHL interpreter is a simple stand-alone PHP interpreter that allows` |
|     - |    8 | ` * the user to enter and execute PHP files against a PH7 engine.` |
|     - |    9 | ` * To start the phl program, just type "phl" followed by the name of the PHP file` |
|     - |   10 | ` * to compile and execute. That is, the first argument is to the interpreter, the rest` |
|     - |   11 | ` * are scripts arguments, press "Enter" and the PHP code will be executed.` |
|     - |   12 | ` * If something goes wrong while processing the PHP script due to a compile-time error` |
|     - |   13 | ` * your error output (STDOUT) should display the compile-time error messages.` |
|     - |   14 | ` *` |
|     - |   15 | ` * Usage example of the phl interpreter:` |
|     - |   16 | ` *   phl hello_world.php` |
|     - |   17 | ` * Running the interpreter with script arguments` |
|     - |   18 | ` *    phl scripts/mp3_tag.php /usr/local/path/to/my_mp3s` |
|     - |   19 | ` *` |
|     - |   20 | ` * Command line options:` |
|     - |   21 | ` *   -b: Dump PH7 byte-code instructions` |
|     - |   22 | ` *   -h: Display this help message` |
|     - |   23 | ` *` |
|     - |   24 | ` * The PHL interpreter package includes more than 70 PHP scripts to test ranging from` |
|     - |   25 | ` * simple hello world programs to XML processing, ZIP archive extracting, MP3 tag extracting,` |
|     - |   26 | ` * UUID generation, JSON encoding/decoding, INI processing, Base32 encoding/decoding and many` |
|     - |   27 | ` * more. These scripts are available in the scripts directory from the zip archive.` |
|     - |   28 | ` */` |
|     - |   29 | `#include <stdio.h>` |
|     - |   30 | `#include <stdlib.h>` |
|     - |   31 | `#include <string.h>` |
|     - |   32 | `#include <time.h>` |
|     - |   33 | `#include <errno.h>` |
|     - |   34 | `#include <locale.h>` |
|     - |   35 | `#ifndef __WINNT__` |
|     - |   36 | `#include <signal.h>   /* SIGPIPE — see main() */` |
|     - |   37 | `#endif` |
|     - |   38 | `#ifdef __UNIXES__` |
|     - |   39 | `#include <unistd.h>` |
|     - |   40 | `#endif` |
|     - |   41 | `/* Make sure this header file is available.*/` |
|     - |   42 | `#include "ph7.h"` |
|     - |   43 | `#ifdef PHL_ENABLE_SERVER` |
|     - |   44 | `#include "server.h"` |
|     - |   45 | `#endif` |
|     - |   46 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|     - |   47 | `#define MINIDUMP_IMPLEMENTATION` |
|     - |   48 | `#include "minidump.h"` |
|     - |   49 | `#endif` |
|     - |   50 | `/*` |
|     - |   51 | ` * Display an error message and exit.` |
|     - |   52 | ` */` |
|   ! 0 |   53 | `static void FatalCode(const char *zMsg,int iCode)` |
|   ! 0 |   54 | `{` |
|   ! 0 |   55 | `	puts(zMsg);` |
|     - |   56 | `	/* Shutdown the library */` |
|   ! 0 |   57 | `	ph7_lib_shutdown();` |
|     - |   58 | `	/* Exit immediately */` |
|   ! 0 |   59 | `	exit(iCode);` |
|   ! 0 |   60 | `}` |
|     - |   61 | `/*` |
|     - |   62 | ` * php-parity default: fatal engine/compile failures exit 255 (php exits 255` |
|     - |   63 | ` * on a fatal compile error); usage and IO errors use FatalCode(msg, 1)` |
|     - |   64 | ` * directly, mirroring php's exit 1 for bad invocations / unopenable input.` |
|     - |   65 | ` */` |
|   ! 0 |   66 | `static void Fatal(const char *zMsg)` |
|   ! 0 |   67 | `{` |
|   ! 0 |   68 | `	FatalCode(zMsg,255);` |
|   ! 0 |   69 | `}` |
|     - |   70 | `/*` |
|     - |   71 | ` * Exit 255 without printing anything: used when the diagnostic has already been` |
|     - |   72 | ` * emitted by the error consumer (a compile/parse error), which is exactly what` |
|     - |   73 | ` * php does — it prints the parse error and nothing more.` |
|     - |   74 | ` */` |
|     - |   75 | `/* The engine this process built, so the silent-fatal exit can give it back:` |
|     - |   76 | ` * ph7_lib_shutdown() releases the LIBRARY, and an engine instance owns a mutex` |
|     - |   77 | ` * of its own. Without this the fatal path leaks it, which is invisible in` |
|     - |   78 | ` * ordinary use (the process is exiting) and turns every leak-detecting run over` |
|     - |   79 | ` * a corpus that spawns a failing child into a wall of reports. */` |
|     - |   80 | `static ph7 *pFatalEngine = 0;` |
|   946 |   81 | `static void FatalSilentCode(int iCode)` |
|     4 |   82 | `{` |
|   950 |   83 | `	if( pFatalEngine ){` |
|   950 |   84 | `		ph7_release(pFatalEngine);` |
|   950 |   85 | `		pFatalEngine = 0;` |
|   473 |   86 | `	}` |
|   950 |   87 | `	ph7_lib_shutdown();` |
|   950 |   88 | `	exit(iCode);` |
|   ! 0 |   89 | `}` |
|   942 |   90 | `static void FatalSilent(void)` |
|     4 |   91 | `{` |
|   946 |   92 | `	FatalSilentCode(255);` |
|   471 |   93 | `}` |
|     - |   94 | `/*` |
|     - |   95 | ` * Display the banner,a help message and exit.` |
|     - |   96 | ` */` |
|     2 |   97 | `static void Help(void)` |
|     2 |   98 | `{` |
|     4 |   99 | `	puts("phl [-h\|--help\|-b\|-i\|-l\|-v\|--version\|-r code\|--rf name\|--rc name\|--re name\|-d name=value\|-c inifile] path/to/php_file [script args]");` |
|     - |  100 | `#ifdef PHL_ENABLE_SERVER` |
|     4 |  101 | `	puts("phl -S host:port [-t docroot] [router.php]");` |
|     - |  102 | `#endif` |
|     4 |  103 | `	puts("\t-b: Dump PH7 byte-code instructions");` |
|     4 |  104 | `	puts("\t-i: Display interpreter information and exit");` |
|     4 |  105 | `	puts("\t-l: Syntax-check (lint) the given file and exit");` |
|     4 |  106 | `	puts("\t-r code: Run code from command line (no tags needed)");` |
|     - |  107 | `#ifdef PHL_ENABLE_SERVER` |
|     4 |  108 | `	puts("\t-S host:port: Start the built-in development server");` |
|     4 |  109 | `	puts("\t-t docroot: Document root for the server (default: current directory)");` |
|     - |  110 | `#endif` |
|     4 |  111 | `	puts("\t-v, --version: Display version information and exit");` |
|     4 |  112 | `	puts("\t-h, --help: Display this message and exit");` |
|     - |  113 | `	/* Exit immediately */` |
|     4 |  114 | `	exit(0);` |
|   ! 0 |  115 | `}` |
|     - |  116 | `/*` |
|     - |  117 | ` * Display version information and exit.` |
|     - |  118 | ` */` |
|     6 |  119 | `static void Version(void)` |
|     1 |  120 | `{` |
|     7 |  121 | `	puts("PHL " PH7_VERSION " (cli) (built " __DATE__ " " __TIME__ ")");` |
|     7 |  122 | `	puts("Copyright (c) 2011-2014 Symisc Systems, 2025 Alexandre Gomes Gaigalas");` |
|     - |  123 | `	/* Exit immediately */` |
|     7 |  124 | `	exit(0);` |
|   ! 0 |  125 | `}` |
|     - |  126 | `/*` |
|     - |  127 | ` * Display interpreter information (php -i) and exit. PHP's CLI -i is plain text` |
|     - |  128 | ` * (the phpinfo() builtin emits HTML, suited to the web SAPI), so this prints a` |
|     - |  129 | ` * concise curated subset on the terminal rather than reusing that builtin.` |
|     - |  130 | ` */` |
|     2 |  131 | `static void Info(void)` |
|   ! 0 |  132 | `{` |
|     2 |  133 | `	printf("phpinfo()\n");` |
|     2 |  134 | `	printf("PHP Version => %s\n\n", PHP_COMPAT_VERSION);` |
|     2 |  135 | `	printf("System => %s\n",` |
|     - |  136 | `#ifdef __WINNT__` |
|     - |  137 | `		"Windows NT"` |
|     - |  138 | `#elif defined(__UNIXES__)` |
|     - |  139 | `		"UNIX-Like"` |
|     - |  140 | `#else` |
|     - |  141 | `		"Other OS"` |
|     - |  142 | `#endif` |
|     - |  143 | `	);` |
|     2 |  144 | `	printf("Build Date => %s %s\n", __DATE__, __TIME__);` |
|     2 |  145 | `	printf("PHL Version => %s\n", PH7_VERSION);` |
|     2 |  146 | `	printf("PHP SAPI => cli\n");` |
|     - |  147 | `	/* Exit immediately */` |
|     2 |  148 | `	exit(0);` |
|   ! 0 |  149 | `}` |
|     - |  150 | `#ifdef __WINNT__` |
|     - |  151 | `#include <Windows.h>` |
|     - |  152 | `#else` |
|     - |  153 | `/* Assume UNIX */` |
|     - |  154 | `#include <unistd.h>` |
|     - |  155 | `#include <limits.h>` |
|     - |  156 | `#endif` |
|     - |  157 | `/*` |
|     - |  158 | ` * The following define is used by the UNIX built and have` |
|     - |  159 | ` * no particular meaning on windows.` |
|     - |  160 | ` */` |
|     - |  161 | `#ifndef STDOUT_FILENO` |
|     - |  162 | `#define STDOUT_FILENO	1` |
|     - |  163 | `#endif` |
|     - |  164 | `#ifndef STDERR_FILENO` |
|     - |  165 | `#define STDERR_FILENO	2` |
|     - |  166 | `#endif` |
|     - |  167 | `#ifndef PATH_MAX` |
|     - |  168 | `#define PATH_MAX 4096` |
|     - |  169 | `#endif` |
|     - |  170 | `static char zPhlBinaryPath[PATH_MAX];` |
|     - |  171 | `/*` |
|     - |  172 | ` * Expand callback for the PHP_BINARY constant.` |
|     - |  173 | ` * pUserData points to the resolved binary path.` |
|     - |  174 | ` */` |
|   545 |  175 | `static void PHL_PhpBinaryConst(ph7_value *pVal,void *pUserData)` |
|     5 |  176 | `{` |
|   550 |  177 | `	ph7_value_string(pVal,(const char *)pUserData,-1);` |
|   550 |  178 | `}` |
|     - |  179 | `/*` |
|     - |  180 | ` * Resolve the absolute path of the running interpreter.` |
|     - |  181 | ` * Falls back to argv[0] verbatim (e.g. bare PATH invocation):` |
|     - |  182 | ` * consumers spawning it again go through the shell, which re-resolves it.` |
|     - |  183 | ` */` |
|  5539 |  184 | `static const char * PHL_ResolveBinaryPath(const char *zArgv0)` |
|     5 |  185 | `{` |
|     - |  186 | `#ifdef __WINNT__` |
|     5 |  187 | `	DWORD nLen = GetModuleFileNameA(0,zPhlBinaryPath,(DWORD)sizeof(zPhlBinaryPath));` |
|     5 |  188 | `	if( nLen > 0 && nLen < sizeof(zPhlBinaryPath) ){` |
|     5 |  189 | `		return zPhlBinaryPath;` |
|     - |  190 | `	}` |
|     - |  191 | `#else` |
|  5539 |  192 | `	if( realpath(zArgv0,zPhlBinaryPath) != 0 ){` |
|  5539 |  193 | `		return zPhlBinaryPath;` |
|     - |  194 | `	}` |
|     - |  195 | `#endif` |
|   ! 0 |  196 | `	return zArgv0;` |
|  2770 |  197 | `}` |
|     - |  198 | `/*` |
|     - |  199 | ` * VM output consumer callback.` |
|     - |  200 | ` * Each time the virtual machine generates some outputs,the following` |
|     - |  201 | ` * function gets called by the underlying virtual machine to consume` |
|     - |  202 | ` * the generated output.` |
|     - |  203 | ` * All this function does is redirecting the VM output to STDOUT.` |
|     - |  204 | ` * This function is registered later via a call to ph7_vm_config()` |
|     - |  205 | ` * with a configuration verb set to: PH7_VM_CONFIG_OUTPUT.` |
|     - |  206 | ` */` |
| 79961 |  207 | `static int Output_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)` |
|     5 |  208 | `{` |
| 39020 |  209 | `	(void)pUserData;` |
|     - |  210 | `#ifdef __WINNT__` |
|     - |  211 | `	BOOL rc;` |
|     5 |  212 | `	rc = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),pOutput,(DWORD)nOutputLen,0,0);` |
|     5 |  213 | `	if( !rc ){` |
|     - |  214 | `		/* Abort processing */` |
|   ! 0 |  215 | `		return PH7_ABORT;` |
|     - |  216 | `	}` |
|     - |  217 | `#else` |
|     - |  218 | `	ssize_t nWr;` |
| 79961 |  219 | `	nWr = write(STDOUT_FILENO,pOutput,nOutputLen);` |
| 79961 |  220 | `	if( nWr < 0 ){` |
|     - |  221 | `		/* Abort processing */` |
|   ! 0 |  222 | `		return PH7_ABORT;` |
|     - |  223 | `	}` |
|     - |  224 | `#endif /* __WINT__ */` |
|     - |  225 | `	/* All done,VM output was redirected to STDOUT */` |
| 79966 |  226 | `	return PH7_OK;` |
| 39025 |  227 | `}` |
|     - |  228 | `/*` |
|     - |  229 | ` * VM diagnostics consumer (PH7_VM_CONFIG_ERR_STREAM): the log copy of a runtime` |
|     - |  230 | ` * warning/notice/deprecation and the uncaught-exception fatal go here — STDERR —` |
|     - |  231 | ` * so program STDOUT stays clean, matching stock CLI php.` |
|     - |  232 | ` */` |
|   882 |  233 | `static int Error_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)` |
|     5 |  234 | `{` |
|   441 |  235 | `	(void)pUserData;` |
|     - |  236 | `#ifdef __WINNT__` |
|     - |  237 | `	BOOL rc;` |
|     5 |  238 | `	rc = WriteFile(GetStdHandle(STD_ERROR_HANDLE),pOutput,(DWORD)nOutputLen,0,0);` |
|     5 |  239 | `	if( !rc ){` |
|     - |  240 | `		/* Abort processing */` |
|   ! 0 |  241 | `		return PH7_ABORT;` |
|     - |  242 | `	}` |
|     - |  243 | `#else` |
|     - |  244 | `	ssize_t nWr;` |
|   882 |  245 | `	nWr = write(STDERR_FILENO,pOutput,nOutputLen);` |
|   882 |  246 | `	if( nWr < 0 ){` |
|     - |  247 | `		/* Abort processing */` |
|   ! 0 |  248 | `		return PH7_ABORT;` |
|     - |  249 | `	}` |
|     - |  250 | `#endif /* __WINNT__ */` |
|     - |  251 | `	/* All done, VM diagnostics were redirected to STDERR */` |
|   887 |  252 | `	return PH7_OK;` |
|   446 |  253 | `}` |
|     - |  254 | `/*` |
|     - |  255 | ` * Parse an unsigned-long testing knob from the environment (PHL_MAX_ALLOC /` |
|     - |  256 | ` * PHL_MAX_INPUT / PHL_MAX_RECURSION / PHL_MAX_NATIVE_DEPTH). Returns 1 and writes` |
|     - |  257 | ` * *pOut on a valid, strictly-positive, fully-numeric value clamped to` |
|     - |  258 | ` * [uFloor, uCeil]; returns` |
|     - |  259 | ` * 0 (leaving *pOut untouched) when the var is unset, empty, non-numeric, has` |
|     - |  260 | ` * trailing garbage, or is zero — so a typo like "-1" or "abc" is ignored` |
|     - |  261 | ` * rather than silently reinterpreted (strtoul would wrap "-1" to ULONG_MAX).` |
|     - |  262 | ` */` |
| 24412 |  263 | `static int PHL_EnvULong(const char *zName,unsigned long uFloor,unsigned long uCeil,unsigned long *pOut)` |
|     5 |  264 | `{` |
| 24417 |  265 | `	const char *zVal = getenv(zName);` |
| 24417 |  266 | `	char *zEnd = 0;` |
|     - |  267 | `	unsigned long uMax;` |
| 24417 |  268 | `	if( zVal == 0 \|\| zVal[0] == 0 ){` |
| 24403 |  269 | `		return 0;` |
|     - |  270 | `	}` |
|     - |  271 | `	/* Reject a leading sign outright: strtoul silently negates "-1" to` |
|     - |  272 | `	 * ULONG_MAX, turning a typo into an effectively-unlimited cap. */` |
|    17 |  273 | `	if( zVal[0] == '-' \|\| zVal[0] == '+' ){` |
|   ! 0 |  274 | `		return 0;` |
|     - |  275 | `	}` |
|    17 |  276 | `	errno = 0;` |
|    17 |  277 | `	uMax = strtoul(zVal,&zEnd,10);` |
|    17 |  278 | `	if( errno != 0 \|\| zEnd == zVal \|\| *zEnd != 0 \|\| uMax == 0 ){` |
|   ! 0 |  279 | `		return 0; /* non-numeric, trailing junk, overflow, or zero */` |
|     - |  280 | `	}` |
|    17 |  281 | `	if( uMax < uFloor ){` |
|   ! 0 |  282 | `		uMax = uFloor;` |
|   ! 0 |  283 | `	}` |
|    17 |  284 | `	if( uMax > uCeil ){` |
|   ! 0 |  285 | `		uMax = uCeil;` |
|   ! 0 |  286 | `	}` |
|    17 |  287 | `	*pOut = uMax;` |
|    17 |  288 | `	return 1;` |
| 12193 |  289 | `}` |
|     - |  290 | `/*` |
|     - |  291 | ` * Apply one "name=value" php.ini directive to the VM (used by -d and each` |
|     - |  292 | ` * -c file line). Trims surrounding whitespace and one layer of quotes off` |
|     - |  293 | ` * the value, php.ini style.` |
|     - |  294 | ` */` |
|   131 |  295 | `static void PHL_ApplyIniPair(ph7_vm *pVm,const char *zPair)` |
|     4 |  296 | `{` |
|     - |  297 | `	char zName[128];` |
|     - |  298 | `	char zValue[512];` |
|   135 |  299 | `	const char *zEq = strchr(zPair,'=');` |
|     - |  300 | `	const char *zEnd;` |
|     - |  301 | `	size_t n;` |
|   135 |  302 | `	if( zEq == 0 ){` |
|     - |  303 | `		/* php: a bare -d name defines the entry with value "1" */` |
|     2 |  304 | `		zEq = zPair + strlen(zPair);` |
|     1 |  305 | `	}` |
|     - |  306 | `	/* name: trim */` |
|   135 |  307 | `	while( *zPair == ' ' \|\| *zPair == '\t' ){ zPair++; }` |
|   135 |  308 | `	zEnd = zEq;` |
|   208 |  309 | `	while( zEnd > zPair && (zEnd[-1] == ' ' \|\| zEnd[-1] == '\t') ){ zEnd--; }` |
|   135 |  310 | `	n = (size_t)(zEnd - zPair);` |
|   135 |  311 | `	if( n == 0 \|\| n >= sizeof(zName) ){` |
|   ! 0 |  312 | `		return;` |
|     - |  313 | `	}` |
|   135 |  314 | `	memcpy(zName,zPair,n);` |
|   135 |  315 | `	zName[n] = 0;` |
|     - |  316 | `	/* value: trim + unquote */` |
|   135 |  317 | `	if( *zEq == '=' ){` |
|   133 |  318 | `		const char *zV = zEq + 1;` |
|     - |  319 | `		const char *zVEnd;` |
|   141 |  320 | `		while( *zV == ' ' \|\| *zV == '\t' ){ zV++; }` |
|   133 |  321 | `		zVEnd = zV + strlen(zV);` |
|   205 |  322 | `		while( zVEnd > zV && (zVEnd[-1] == ' ' \|\| zVEnd[-1] == '\t'` |
|   141 |  323 | `		    \|\| zVEnd[-1] == '\r' \|\| zVEnd[-1] == '\n') ){ zVEnd--; }` |
|   133 |  324 | `		if( zVEnd - zV >= 2 && (zV[0] == '"' \|\| zV[0] == '\'') && zVEnd[-1] == zV[0] ){` |
|     4 |  325 | `			zV++;` |
|     4 |  326 | `			zVEnd--;` |
|     2 |  327 | `		}` |
|   133 |  328 | `		n = (size_t)(zVEnd - zV);` |
|   133 |  329 | `		if( n >= sizeof(zValue) ){` |
|   ! 0 |  330 | `			n = sizeof(zValue) - 1;` |
|   ! 0 |  331 | `		}` |
|   133 |  332 | `		memcpy(zValue,zV,n);` |
|   133 |  333 | `		zValue[n] = 0;` |
|    68 |  334 | `	}else{` |
|     - |  335 | `		/* default "on" flag; avoid strcpy (MSVC C4996 under /WX) */` |
|     2 |  336 | `		zValue[0] = '1';` |
|     2 |  337 | `		zValue[1] = 0;` |
|     - |  338 | `	}` |
|   135 |  339 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_INI_ENTRY,zName,zValue);` |
|    69 |  340 | `}` |
|     - |  341 | `/*` |
|     - |  342 | ` * Load php.ini directives from a -c file: name=value lines; [sections],` |
|     - |  343 | ` * empty lines and ;/# comments are ignored (enough of php's ini grammar` |
|     - |  344 | ` * for CLI configuration).` |
|     - |  345 | ` */` |
|     4 |  346 | `static void PHL_LoadIniFile(ph7_vm *pVm,const char *zPath)` |
|   ! 0 |  347 | `{` |
|     - |  348 | `	char zLine[768];` |
|     4 |  349 | `	FILE *pFile = fopen(zPath,"r");` |
|     4 |  350 | `	if( pFile == 0 ){` |
|   ! 0 |  351 | `		fprintf(stderr,"Could not open php.ini file: %s\n",zPath);` |
|   ! 0 |  352 | `		return;` |
|     - |  353 | `	}` |
|    20 |  354 | `	while( fgets(zLine,sizeof(zLine),pFile) ){` |
|    16 |  355 | `		const char *z = zLine;` |
|    16 |  356 | `		while( *z == ' ' \|\| *z == '\t' ){ z++; }` |
|    16 |  357 | `		if( *z == 0 \|\| *z == ';' \|\| *z == '#' \|\| *z == '[' \|\| *z == '\n' \|\| *z == '\r' ){` |
|     8 |  358 | `			continue;` |
|     - |  359 | `		}` |
|     8 |  360 | `		PHL_ApplyIniPair(pVm,z);` |
|   ! 0 |  361 | `	}` |
|     4 |  362 | `	fclose(pFile);` |
|     2 |  363 | `}` |
|     - |  364 | `/*` |
|     - |  365 | ` * Return TRUE when standard input is a pipe/redirect rather than an interactive` |
|     - |  366 | ` * terminal. php reads the script from stdin in exactly that case; a terminal` |
|     - |  367 | ` * would block waiting for input, so there we keep the usage error instead.` |
|     - |  368 | ` */` |
|    10 |  369 | `static int PHL_StdinIsPipe(void)` |
|     2 |  370 | `{` |
|     - |  371 | `#ifdef __UNIXES__` |
|    10 |  372 | `	return !isatty(0);` |
|     - |  373 | `#else` |
|     2 |  374 | `	return 0;` |
|     - |  375 | `#endif` |
|     2 |  376 | `}` |
|     - |  377 | `/*` |
|     - |  378 | ` * Slurp all of standard input into a heap buffer (NUL-terminated). Returns the` |
|     - |  379 | ` * buffer (caller owns it, though the process exits shortly after) and writes the` |
|     - |  380 | ` * byte length to *pnLen, or NULL on allocation failure.` |
|     - |  381 | ` */` |
|    12 |  382 | `static char * PHL_SlurpStdin(int *pnLen)` |
|   ! 0 |  383 | `{` |
|    12 |  384 | `	size_t nCap = 8192, nUsed = 0;` |
|    12 |  385 | `	char *zBuf = (char *)malloc(nCap);` |
|    12 |  386 | `	if( zBuf == 0 ){ return 0; }` |
|     6 |  387 | `	for(;;){` |
|     - |  388 | `		size_t nRd;` |
|    12 |  389 | `		if( nUsed + 4096 + 1 > nCap ){` |
|     - |  390 | `			char *zNew;` |
|   ! 0 |  391 | `			nCap *= 2;` |
|   ! 0 |  392 | `			zNew = (char *)realloc(zBuf, nCap);` |
|   ! 0 |  393 | `			if( zNew == 0 ){ free(zBuf); return 0; }` |
|   ! 0 |  394 | `			zBuf = zNew;` |
|   ! 0 |  395 | `		}` |
|    12 |  396 | `		nRd = fread(zBuf + nUsed, 1, 4096, stdin);` |
|    12 |  397 | `		nUsed += nRd;` |
|    12 |  398 | `		if( nRd < 4096 ){ break; }` |
|   ! 0 |  399 | `	}` |
|    12 |  400 | `	zBuf[nUsed] = 0;` |
|    12 |  401 | `	*pnLen = (int)nUsed;` |
|    12 |  402 | `	return zBuf;` |
|     6 |  403 | `}` |
|     - |  404 | `/*` |
|     - |  405 | ` * Main program: Compile and execute the PHP file.` |
|     - |  406 | ` */` |
|  7156 |  407 | `int main(int argc,char **argv)` |
|     5 |  408 | `{` |
|     - |  409 | `	ph7 *pEngine; /* PH7 engine */` |
|     - |  410 | `	ph7_vm *pVm;  /* Compiled PHP program */` |
|  7161 |  411 | `	int dump_vm = 0;    /* Dump VM instructions if TRUE */` |
|  7161 |  412 | `	int run_code = 0;    /* Run inline code if TRUE */` |
|  7161 |  413 | `	int lint_mode = 0;   /* Syntax-check only (-l) if TRUE */` |
|  7161 |  414 | `	const char *zRunCode = 0; /* Inline code string */` |
|  7161 |  415 | `	int stdin_code = 0;       /* Execute a script read from stdin if TRUE */` |
|  7161 |  416 | `	char *zStdinCode = 0;     /* Script slurped from stdin */` |
|  7161 |  417 | `	int nStdinCode = 0;       /* Length of the stdin script */` |
|  7161 |  418 | ``	int dash_dash = 0;        /* Saw `--`: read from stdin, rest are script args */`` |
|     - |  419 | `#ifdef PHL_ENABLE_SERVER` |
|  7161 |  420 | `	int server_mode = 0;        /* Start built-in server if TRUE */` |
|  7161 |  421 | `	const char *zServerAddr = 0; /* host:port string */` |
|  7161 |  422 | `	const char *zDocRoot = ".";  /* Document root */` |
|     - |  423 | `#endif` |
|     - |  424 | `	int n;              /* Script arguments */` |
|     - |  425 | `	int rc;` |
|     - |  426 | `	const char *azIniDefine[64]; /* -d name=value directives, in order */` |
|  7161 |  427 | `	int nIniDefine = 0;` |
|  7161 |  428 | `	const char *zIniFile = 0;    /* -c php.ini path */` |
|     - |  429 | `	/* php's CLI ignores SIGPIPE for the whole process, and this is PHL's CLI: a` |
|     - |  430 | `	 * write to a pipe or socket whose reader is gone then answers EPIPE instead of` |
|     - |  431 | `	 * KILLING the interpreter, which is the answer every php program is written` |
|     - |  432 | `	 * against. The disposition is inherited across fork AND exec, so it is also` |
|     - |  433 | ``	 * what a proc_open() child runs with -- `cat` writing into a pipe the script`` |
|     - |  434 | `	 * has closed exits 1 with a write error under php, where a default SIGPIPE` |
|     - |  435 | `	 * kills it and proc_close() reports 141. The networking subsystem used to set` |
|     - |  436 | `	 * this lazily at the first socket, so a program that opened none never had it. */` |
|     - |  437 | `#if defined(SIGPIPE) && defined(SIG_IGN)` |
|  7156 |  438 | `	signal(SIGPIPE,SIG_IGN);` |
|     - |  439 | `#endif` |
|     - |  440 | `	/* php's own module startup pins LC_CTYPE to C.UTF-8 and leaves every other` |
|     - |  441 | `` 	 * category at C, whatever the environment says -- so `setlocale(LC_CTYPE,'0')` `` |
|     - |  442 | `	 * answers "C.UTF-8" under php on a box with the locale and "C" on one without,` |
|     - |  443 | `	 * and never the LANG the shell exported. That is observable on its own, and it` |
|     - |  444 | `	 * also decides the encoding ext/gettext hands an answer back in when nothing` |
|     - |  445 | `	 * called bind_textdomain_codeset(). The engine LIBRARY does not touch the` |
|     - |  446 | `	 * process locale; this is the CLI, which is PHL's SAPI. */` |
|  7161 |  447 | `	if( setlocale(LC_CTYPE,"C.UTF-8") == 0 ){` |
|     5 |  448 | `		setlocale(LC_CTYPE,"C");` |
|   ! 0 |  449 | `	}` |
|     - |  450 | `	/* Process interpreter arguments first*/` |
|  7647 |  451 | `	for(n = 1 ; n < argc ; ++n ){` |
|     - |  452 | `		int c;` |
|  7145 |  453 | `		if( argv[n][0] != '-' ){` |
|     - |  454 | `			/* No more interpreter arguments */` |
|  6652 |  455 | `			break;` |
|     - |  456 | `		}` |
|     - |  457 | `		/* Check for long options */` |
|   497 |  458 | `		if( argv[n][1] == '-' ){` |
|    25 |  459 | `			if( argv[n][2] == 0 ){` |
|     - |  460 | ``				/* php CLI parity: a bare `--` ends interpreter options; the`` |
|     - |  461 | ``				 * script is read from stdin and everything after `--` becomes`` |
|     - |  462 | `				 * the script's own arguments ($argv[1..]). */` |
|     2 |  463 | `				dash_dash = 1;` |
|     2 |  464 | `				n++;` |
|     2 |  465 | `				break;` |
|     - |  466 | `			}` |
|    23 |  467 | `			if( strcmp(argv[n], "--version") == 0 ){` |
|     7 |  468 | `				Version();` |
|    19 |  469 | `			}else if( strcmp(argv[n], "--help") == 0 ){` |
|     3 |  470 | `				Help();` |
|    15 |  471 | `			}else if( strcmp(argv[n], "--rf") == 0 \|\| strcmp(argv[n], "--rc") == 0 ){` |
|     - |  472 | ``				/* php CLI parity: `--rf <function>` / `--rc <class>` print the`` |
|     - |  473 | `				 * Reflection export (the __toString machinery is byte-exact vs` |
|     - |  474 | ``				 * php) and exit 1 with `Exception: <message>` when the target`` |
|     - |  475 | `				 * does not exist. Implemented as an inline snippet riding the` |
|     - |  476 | `				 * -r code path; the NAME is charset-validated (identifier +` |
|     - |  477 | `				 * namespace separators) so it embeds safely in the snippet. */` |
|     - |  478 | `				static char zReflCode[768];` |
|     7 |  479 | `				const char *zWhat = (argv[n][3] == 'f') ? "ReflectionFunction" : "ReflectionClass";` |
|     - |  480 | `				const char *zName;` |
|     - |  481 | `				const char *zChk;` |
|     7 |  482 | `				if( n + 1 >= argc ){` |
|   ! 0 |  483 | `					FatalCode("Missing name argument for --rf/--rc",1);` |
|   ! 0 |  484 | `				}` |
|     7 |  485 | `				zName = argv[++n];` |
|    71 |  486 | `				for( zChk = zName ; *zChk ; zChk++ ){` |
|    65 |  487 | `					char ch = *zChk;` |
|    65 |  488 | `					if( !(ch == '_' \|\| ch == '\\'` |
|    58 |  489 | `					   \|\| (ch >= 'a' && ch <= 'z') \|\| (ch >= 'A' && ch <= 'Z')` |
|     4 |  490 | `					   \|\| (ch >= '0' && ch <= '9')) ){` |
|   ! 0 |  491 | `						FatalCode("Invalid name for --rf/--rc",1);` |
|   ! 0 |  492 | `					}` |
|    33 |  493 | `				}` |
|     7 |  494 | `				if( strlen(zName) > 250 ){` |
|   ! 0 |  495 | `					FatalCode("Name too long for --rf/--rc",1);` |
|   ! 0 |  496 | `				}` |
|     - |  497 | `				{` |
|     - |  498 | `					/* Double the namespace separators: inside the snippet's` |
|     - |  499 | `					 * double-quoted string a lone backslash could form an` |
|     - |  500 | `					 * escape sequence ("App\name" -> newline). */` |
|     - |  501 | `					static char zEsc[512];` |
|     7 |  502 | `					char *pOut = zEsc;` |
|    71 |  503 | `					for( zChk = zName ; *zChk ; zChk++ ){` |
|    65 |  504 | `						if( *zChk == '\\' ){` |
|   ! 0 |  505 | `							*pOut++ = '\\';` |
|   ! 0 |  506 | `						}` |
|    65 |  507 | `						*pOut++ = *zChk;` |
|    33 |  508 | `					}` |
|     7 |  509 | `					*pOut = 0;` |
|     7 |  510 | `					snprintf(zReflCode,sizeof(zReflCode),` |
|     - |  511 | `						"try { echo new %s(\"%s\"), \"\\n\"; } catch (Throwable $e) { echo \"Exception: \", $e->getMessage(), \"\\n\"; exit(1); }",` |
|     - |  512 | `						zWhat,zEsc);` |
|     - |  513 | `				}` |
|     7 |  514 | `				zRunCode = zReflCode;` |
|     7 |  515 | `				run_code = 1;` |
|    10 |  516 | `			}else if( strcmp(argv[n], "--re") == 0 ){` |
|     - |  517 | ``				/* php CLI parity: `--re <extension>` prints the module's whole`` |
|     - |  518 | `				 * Reflection export. An extension NAME is not an identifier --` |
|     - |  519 | ``				 * php has `Zend OPcache` and `pdo_sqlite` -- so the charset is`` |
|     - |  520 | `				 * --rz's, and the refusal is ReflectionExtension's own. */` |
|     - |  521 | `				static char zExtCode[768];` |
|     - |  522 | `				const char *zName;` |
|     - |  523 | `				const char *zChk;` |
|     5 |  524 | `				if( n + 1 >= argc ){` |
|   ! 0 |  525 | `					FatalCode("Missing name argument for --re",1);` |
|   ! 0 |  526 | `				}` |
|     5 |  527 | `				zName = argv[++n];` |
|    45 |  528 | `				for( zChk = zName ; *zChk ; zChk++ ){` |
|    41 |  529 | `					char ch = *zChk;` |
|    41 |  530 | `					if( !(ch == '_' \|\| ch == ' ' \|\| ch == '.' \|\| ch == '-'` |
|    34 |  531 | `					   \|\| (ch >= 'a' && ch <= 'z') \|\| (ch >= 'A' && ch <= 'Z')` |
|   ! 0 |  532 | `					   \|\| (ch >= '0' && ch <= '9')) ){` |
|   ! 0 |  533 | `						FatalCode("Invalid name for --re",1);` |
|   ! 0 |  534 | `					}` |
|    21 |  535 | `				}` |
|     5 |  536 | `				if( strlen(zName) > 250 ){` |
|   ! 0 |  537 | `					FatalCode("Name too long for --re",1);` |
|   ! 0 |  538 | `				}` |
|     5 |  539 | `				snprintf(zExtCode,sizeof(zExtCode),` |
|     - |  540 | `					"try { echo new ReflectionExtension(\"%s\"), \"\n\"; }"` |
|     - |  541 | `					" catch (Throwable $e) { echo \"Exception: \", $e->getMessage(), \"\n\"; exit(1); }",` |
|     - |  542 | `					zName);` |
|     5 |  543 | `				zRunCode = zExtCode;` |
|     5 |  544 | `				run_code = 1;` |
|     5 |  545 | `			}else if( strcmp(argv[n], "--rz") == 0 ){` |
|     - |  546 | ``				/* php CLI parity: `--rz <name>` reflects a ZEND extension. PHL`` |
|     - |  547 | `				 * loads none, so every name is the refusal php prints for one` |
|     - |  548 | `				 * it does not have -- which is ReflectionZendExtension's own` |
|     - |  549 | `				 * constructor, reached the way --rf/--rc reach theirs. */` |
|     - |  550 | `				static char zZendCode[768];` |
|     - |  551 | `				const char *zName;` |
|     - |  552 | `				const char *zChk;` |
|     3 |  553 | `				if( n + 1 >= argc ){` |
|   ! 0 |  554 | `					FatalCode("Missing name argument for --rz",1);` |
|   ! 0 |  555 | `				}` |
|     3 |  556 | `				zName = argv[++n];` |
|    33 |  557 | `				for( zChk = zName ; *zChk ; zChk++ ){` |
|    31 |  558 | `					char ch = *zChk;` |
|    31 |  559 | `					if( !(ch == '_' \|\| ch == ' ' \|\| ch == '.' \|\| ch == '-'` |
|    30 |  560 | `					   \|\| (ch >= 'a' && ch <= 'z') \|\| (ch >= 'A' && ch <= 'Z')` |
|   ! 0 |  561 | `					   \|\| (ch >= '0' && ch <= '9')) ){` |
|   ! 0 |  562 | `						FatalCode("Invalid name for --rz",1);` |
|   ! 0 |  563 | `					}` |
|    16 |  564 | `				}` |
|     3 |  565 | `				if( strlen(zName) > 250 ){` |
|   ! 0 |  566 | `					FatalCode("Name too long for --rz",1);` |
|   ! 0 |  567 | `				}` |
|     3 |  568 | `				snprintf(zZendCode,sizeof(zZendCode),` |
|     - |  569 | `					"try { echo new ReflectionZendExtension(\"%s\"), \"\n\"; }"` |
|     - |  570 | `					" catch (Throwable $e) { echo \"Exception: \", $e->getMessage(), \"\n\"; exit(1); }",` |
|     - |  571 | `					zName);` |
|     3 |  572 | `				zRunCode = zZendCode;` |
|     3 |  573 | `				run_code = 1;` |
|     2 |  574 | `			}else{` |
|     - |  575 | `				/* Unknown long option */` |
|   ! 0 |  576 | `				Help();` |
|     - |  577 | `			}` |
|    18 |  578 | `			continue;` |
|     - |  579 | `		}` |
|   475 |  580 | `		c = argv[n][1];` |
|   475 |  581 | `		if( c == 'b' ){` |
|     - |  582 | `			/* Dump byte-code instructions */` |
|     3 |  583 | `			dump_vm = 1;` |
|   474 |  584 | `		}else if( c == 'l' ){` |
|     - |  585 | `			/* Syntax-check only (lint) the file argument that follows */` |
|   248 |  586 | `			lint_mode = 1;` |
|   350 |  587 | `		}else if( c == 'i' ){` |
|     - |  588 | `			/* Display interpreter information and exit */` |
|     2 |  589 | `			Info();` |
|   226 |  590 | `		}else if( c == 'm' ){` |
|     - |  591 | ``			/* php CLI parity: `-m` lists the loaded modules, php's two sections`` |
|     - |  592 | `			 * and its case-insensitive order. PHL loads no Zend extension, so` |
|     - |  593 | `			 * that section is always the header and nothing under it -- which` |
|     - |  594 | `			 * is php's own shape for a build with none. Rides the -r code path` |
|     - |  595 | `			 * the way --rf/--rc do. */` |
|     3 |  596 | `			zRunCode =` |
|     - |  597 | `				"$m = get_loaded_extensions();"` |
|     - |  598 | `				"usort($m, 'strcasecmp');"` |
|     - |  599 | `				"echo \"[PHP Modules]\n\";"` |
|     - |  600 | `				"foreach ($m as $x) { echo $x, \"\n\"; }"` |
|     - |  601 | `				"echo \"\n[Zend Modules]\n\";"` |
|     - |  602 | `				"foreach (get_loaded_extensions(true) as $x) { echo $x, \"\n\"; }"` |
|     - |  603 | `				"echo \"\n\";";` |
|     3 |  604 | `			run_code = 1;` |
|   224 |  605 | `		}else if( c == 'f' ){` |
|     - |  606 | ``			/* php CLI parity: `-f <file>` explicitly names the script to run.`` |
|     - |  607 | `			 * The path follows as the next argument, which the positional` |
|     - |  608 | `			 * file handling below already consumes, so treat -f as a no-op. */` |
|   ! 0 |  609 | `			continue;` |
|   223 |  610 | `		}else if( c == 'r' ){` |
|     - |  611 | `			/* Run inline PHP code from next argument (php -r style) */` |
|    31 |  612 | `			if( n + 1 >= argc ){` |
|     - |  613 | `				/* Missing code argument */` |
|   ! 0 |  614 | `				FatalCode("Missing code argument for -r",1);` |
|   ! 0 |  615 | `			}` |
|    31 |  616 | `			zRunCode = argv[++n];` |
|    31 |  617 | `			run_code = 1;` |
|   209 |  618 | `		}else if( c == 'S' ){` |
|     - |  619 | `			/* Start built-in development server */` |
|     - |  620 | `#ifdef PHL_ENABLE_SERVER` |
|    32 |  621 | `			if( n + 1 >= argc ){` |
|   ! 0 |  622 | `				FatalCode("Missing host:port argument for -S",1);` |
|   ! 0 |  623 | `			}` |
|    32 |  624 | `			zServerAddr = argv[++n];` |
|    32 |  625 | `			server_mode = 1;` |
|     - |  626 | `#else` |
|     - |  627 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|     - |  628 | `#endif` |
|   179 |  629 | `		}else if( c == 't' ){` |
|     - |  630 | `			/* Set document root for the server */` |
|     - |  631 | `#ifdef PHL_ENABLE_SERVER` |
|    32 |  632 | `			if( n + 1 >= argc ){` |
|   ! 0 |  633 | `				FatalCode("Missing docroot argument for -t",1);` |
|   ! 0 |  634 | `			}` |
|    32 |  635 | `			zDocRoot = argv[++n];` |
|     - |  636 | `#else` |
|     - |  637 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|     - |  638 | `#endif` |
|   147 |  639 | `		}else if( c == 'v' ){` |
|     - |  640 | `			/* Display version */` |
|   ! 0 |  641 | `			Version();` |
|   131 |  642 | `		}else if( c == 'd' ){` |
|     - |  643 | `			/* php CLI parity: -d name=value defines a php.ini entry` |
|     - |  644 | `			 * (repeatable; applied to the VM after compile, in order). */` |
|   127 |  645 | `			if( n + 1 >= argc ){` |
|   ! 0 |  646 | `				FatalCode("Missing name=value argument for -d",1);` |
|   ! 0 |  647 | `			}` |
|   127 |  648 | `			if( nIniDefine < (int)(sizeof(azIniDefine)/sizeof(azIniDefine[0])) ){` |
|   127 |  649 | `				azIniDefine[nIniDefine++] = argv[++n];` |
|    65 |  650 | `			}else{` |
|   ! 0 |  651 | `				FatalCode("Too many -d directives",1);` |
|     4 |  652 | `			}` |
|    65 |  653 | `		}else if( c == 'c' ){` |
|     - |  654 | `			/* php CLI parity: -c file loads php.ini directives from a file` |
|     - |  655 | `			 * (name=value lines; [sections] and ;/# comments ignored). */` |
|     4 |  656 | `			if( n + 1 >= argc ){` |
|   ! 0 |  657 | `				FatalCode("Missing file argument for -c",1);` |
|   ! 0 |  658 | `			}` |
|     4 |  659 | `			zIniFile = argv[++n];` |
|     2 |  660 | `		}else{` |
|     - |  661 | `			/* Display a help message and exit */` |
|   ! 0 |  662 | `			Help();` |
|     - |  663 | `		}` |
|   239 |  664 | `	}` |
|     - |  665 | `#ifdef PHL_ENABLE_SERVER` |
|  7156 |  666 | `	if( server_mode ){` |
|     - |  667 | `		/* Parse host:port from zServerAddr */` |
|     - |  668 | `		char zHost[256];` |
|    32 |  669 | `		int iPort = 0;` |
|     - |  670 | `		const char *zColon;` |
|    32 |  671 | `		const char *zRouter = 0;` |
|    32 |  672 | `		zColon = strrchr(zServerAddr, ':');` |
|    32 |  673 | `		if( zColon == 0 ){` |
|   ! 0 |  674 | `			FatalCode("Invalid address format. Use host:port (e.g., localhost:8080)",1);` |
|   ! 0 |  675 | `		}` |
|     - |  676 | `		{` |
|    32 |  677 | `			int nHostLen = (int)(zColon - zServerAddr);` |
|    32 |  678 | `			if( nHostLen >= (int)sizeof(zHost) ) nHostLen = (int)sizeof(zHost) - 1;` |
|    32 |  679 | `			memcpy(zHost, zServerAddr, nHostLen);` |
|    32 |  680 | `			zHost[nHostLen] = 0;` |
|     - |  681 | `		}` |
|    32 |  682 | `		iPort = atoi(zColon + 1);` |
|    32 |  683 | `		if( iPort <= 0 \|\| iPort > 65535 ){` |
|   ! 0 |  684 | `			FatalCode("Invalid port number",1);` |
|   ! 0 |  685 | `		}` |
|     - |  686 | `		/* Check for optional router script */` |
|    32 |  687 | `		if( n < argc ){` |
|   ! 0 |  688 | `			zRouter = argv[n];` |
|   ! 0 |  689 | `		}` |
|    32 |  690 | `		return phl_serve(zHost, iPort, zDocRoot, zRouter, PHL_ResolveBinaryPath(argv[0]));` |
|     - |  691 | `	}` |
|     - |  692 | `#endif` |
|  7124 |  693 | `	if( (n >= argc \|\| dash_dash) && !run_code ){` |
|     - |  694 | ``		/* No file and no -r: php reads the script from stdin. `--` forces this`` |
|     - |  695 | `		 * (rest are script args); otherwise only when stdin is a pipe/redirect` |
|     - |  696 | `		 * (an interactive terminal would just block). */` |
|    14 |  697 | `		if( dash_dash \|\| PHL_StdinIsPipe() ){` |
|    12 |  698 | `			zStdinCode = PHL_SlurpStdin(&nStdinCode);` |
|    12 |  699 | `			if( zStdinCode == 0 ){` |
|   ! 0 |  700 | `				FatalCode("IO error while reading standard input",1);` |
|   ! 0 |  701 | `			}` |
|    12 |  702 | `			stdin_code = 1;` |
|     6 |  703 | `		}else{` |
|     2 |  704 | `			puts("Missing PHP file to compile");` |
|     2 |  705 | `			Help();` |
|     - |  706 | `		}` |
|     6 |  707 | `	}` |
|     - |  708 |  |
|     - |  709 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|     - |  710 | `	/* Install an unhandled exception minidump handler for Windows debug builds */` |
|     5 |  711 | `	CreateMiniDumpOnUnHandledException();` |
|     - |  712 | `#endif` |
|     - |  713 | `	/* Allocate a new PH7 engine instance */` |
|  6234 |  714 | `	rc = ph7_init(&pEngine);` |
|  6234 |  715 | `	pFatalEngine = pEngine;` |
|  6234 |  716 | `	if( rc != PH7_OK ){` |
|     - |  717 | `		/*` |
|     - |  718 | `		 * If the supplied memory subsystem is so sick that we are unable` |
|     - |  719 | `		 * to allocate a tiny chunk of memory,there is no much we can do here.` |
|     - |  720 | `		 */` |
|   ! 0 |  721 | `		Fatal("Error while allocating a new PH7 engine instance");` |
|   ! 0 |  722 | `	}` |
|     - |  723 | `	/* Set an error log consumer callback. This callback [Output_Consumer()] will` |
|     - |  724 | `	 * redirect all compile-time error messages to STDOUT.` |
|     - |  725 | `	 */` |
|  6234 |  726 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,` |
|     - |  727 | `		Output_Consumer, /* Error log consumer */` |
|     - |  728 | `		0 /* NULL: Callback Private data */` |
|     - |  729 | `		);` |
|     - |  730 | `	/* Optional per-allocation memory cap (PHL_MAX_ALLOC=bytes). Used to` |
|     - |  731 | `	 * deterministically exercise out-of-memory paths (see tests/ph7/003-stress).` |
|     - |  732 | `	 * Clamp to a floor above the pool bucket size (SXMEM_POOL_MAXALLOC, 32 KB)` |
|     - |  733 | `	 * so the engine can still start; VMs inherit it at creation. */` |
|     - |  734 | `	{` |
|     - |  735 | `		unsigned long uMax;` |
|     - |  736 | `		/* floor: keep above the pool bucket size; clamp: nMaxRequest is 32-bit */` |
|  6234 |  737 | `		if( PHL_EnvULong("PHL_MAX_ALLOC",65536UL,0xFFFFFFFFUL,&uMax) ){` |
|   ! 0 |  738 | `			ph7_config(pEngine,PH7_CONFIG_MAX_ALLOC,(unsigned int)uMax);` |
|   ! 0 |  739 | `		}` |
|     - |  740 | `	}` |
|     - |  741 | `	/* Optional per-input byte cap (PHL_MAX_INPUT=bytes). Used to exercise the` |
|     - |  742 | `	 * input-size rejection path at a manageable scale (see tests/ph7/003-stress). */` |
|     - |  743 | `	{` |
|     - |  744 | `		unsigned long uMax;` |
|  6234 |  745 | `		if( PHL_EnvULong("PHL_MAX_INPUT",1UL,0xFFFFFFFFUL,&uMax) ){` |
|   ! 0 |  746 | `			ph7_config(pEngine,PH7_CONFIG_MAX_INPUT,(unsigned int)uMax);` |
|   ! 0 |  747 | `		}` |
|     - |  748 | `	}` |
|     - |  749 | `	/* Syntax-check only mode (-l): compile the target file, print PHP's summary` |
|     - |  750 | `	 * line and exit without executing. The error consumer installed above` |
|     - |  751 | `	 * already prints any parse error; ph7_compile_file leaves *pVm NULL on a` |
|     - |  752 | `	 * compile/IO error, so only a successful compile owns a VM to release. */` |
|  6234 |  753 | `	if( lint_mode ){` |
|     - |  754 | `		const char *zFile;` |
|   248 |  755 | `		if( n >= argc ){` |
|     - |  756 | ``			/* No file argument (e.g. `-l` alone, or `-l` mixed with `-r`). */`` |
|   ! 0 |  757 | `			ph7_release(pEngine);` |
|   ! 0 |  758 | `			pFatalEngine = 0;` |
|   ! 0 |  759 | `			puts("No input file specified");` |
|   ! 0 |  760 | `			return 255;` |
|     - |  761 | `		}` |
|   248 |  762 | `		zFile = argv[n];` |
|   248 |  763 | `		rc = ph7_compile_file(pEngine,zFile,&pVm,PH7_SYNTAX_CHECK);` |
|   248 |  764 | `		if( rc == PH7_OK ){` |
|    78 |  765 | `			printf("No syntax errors detected in %s\n",zFile);` |
|    78 |  766 | `			ph7_vm_release(pVm);` |
|   209 |  767 | `		}else if( rc == PH7_IO_ERR ){` |
|     - |  768 | `			/* php says this on STDERR, like the run path above */` |
|     4 |  769 | `			fprintf(stderr,"Could not open input file: %s\n",zFile);` |
|     2 |  770 | `		}else{` |
|   167 |  771 | `			printf("Errors parsing %s\n",zFile);` |
|     - |  772 | `		}` |
|   248 |  773 | `		ph7_release(pEngine);` |
|   248 |  774 | `		pFatalEngine = 0;` |
|     - |  775 | `		/* php's two exit codes are not one: a file it cannot OPEN is 1 (a bad` |
|     - |  776 | `		 * invocation), a file that does not PARSE is 255. */` |
|   248 |  777 | `		if( rc == PH7_OK ){` |
|    78 |  778 | `			return 0;` |
|     - |  779 | `		}` |
|   171 |  780 | `		return (rc == PH7_IO_ERR) ? 1 : 255;` |
|     - |  781 | `	}` |
|     - |  782 | `	/* Now,it's time to compile our PHP file */` |
|  5988 |  783 | `	if( run_code ){` |
|     - |  784 | `		/* Compile inline PHP code string (PHP only - no tags needed) */` |
|    45 |  785 | `		rc = ph7_compile_v2(` |
|    21 |  786 | `			pEngine, /* PH7 Engine */` |
|    21 |  787 | `			zRunCode, /* Source code */` |
|     - |  788 | `			-1,       /* Let API compute length */` |
|     - |  789 | `			&pVm,     /* OUT: Compiled PHP program */` |
|     - |  790 | `			PH7_PHP_ONLY /* Inline PHP, no tags expected */` |
|     - |  791 | `			);` |
|    45 |  792 | `		if( rc != PH7_OK ){ /* Compile error */` |
|     7 |  793 | `			if( rc == PH7_VM_ERR ){` |
|   ! 0 |  794 | `				Fatal("VM initialization error");` |
|   ! 0 |  795 | `			}else{` |
|     - |  796 | `				/* Compile-time error. The diagnostic has already been printed by the` |
|     - |  797 | `				 * error consumer; php adds nothing else, it just exits 255. */` |
|     7 |  798 | `				FatalSilent();` |
|     - |  799 | `			}` |
|     6 |  800 | `		}` |
|  5967 |  801 | `	}else if( stdin_code ){` |
|     - |  802 | `		/* Script read from stdin: compile it like a file (PHP tags expected). */` |
|    12 |  803 | `		rc = ph7_compile_v2(` |
|     6 |  804 | `			pEngine,     /* PH7 Engine */` |
|     6 |  805 | `			zStdinCode,  /* Source code slurped from stdin */` |
|     6 |  806 | `			nStdinCode,  /* Its byte length */` |
|     - |  807 | `			&pVm,        /* OUT: Compiled PHP program */` |
|     - |  808 | `			0            /* IN: tag mode, like a file */` |
|     - |  809 | `			);` |
|    12 |  810 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   ! 0 |  811 | `			if( rc == PH7_VM_ERR ){` |
|   ! 0 |  812 | `				Fatal("VM initialization error");` |
|   ! 0 |  813 | `			}else{` |
|   ! 0 |  814 | `				FatalSilent();` |
|     - |  815 | `			}` |
|   ! 0 |  816 | `		}` |
|     6 |  817 | `	}else{` |
|  5934 |  818 | `		rc = ph7_compile_file(` |
|  2725 |  819 | `			pEngine, /* PH7 Engine */` |
|  5929 |  820 | `			argv[n], /* Path to the PHP file to compile */` |
|     - |  821 | `			&pVm,    /* OUT: Compiled PHP program */` |
|     - |  822 | `			0        /* IN: Compile flags */` |
|     - |  823 | `			);` |
|  5934 |  824 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   944 |  825 | `			if( rc == PH7_IO_ERR ){` |
|     - |  826 | `				/* php names the file it could not open, and says so on STDERR --` |
|     - |  827 | `				 * this answered a generic "IO error while opening the target file"` |
|     - |  828 | `				 * on stdout, which a script capturing program output then read as` |
|     - |  829 | `				 * part of the answer. */` |
|     4 |  830 | `				fprintf(stderr,"Could not open input file: %s\n",argv[n]);` |
|     4 |  831 | `				FatalSilentCode(1);` |
|   942 |  832 | `			}else if( rc == PH7_VM_ERR ){` |
|   ! 0 |  833 | `				Fatal("VM initialization error");` |
|   ! 0 |  834 | `			}else{` |
|     - |  835 | `				/* Compile-time error. The diagnostic has already been printed by the` |
|     - |  836 | `				 * error consumer; php prints nothing further and exits 255. */` |
|   940 |  837 | `				FatalSilent();` |
|     - |  838 | `			}` |
|   470 |  839 | `		}` |
|     - |  840 | `	}` |
|     - |  841 | `	/*` |
|     - |  842 | `	 * Now we have our script compiled,it's time to configure our VM.` |
|     - |  843 | `	 * We will install the VM output consumer callback defined above` |
|     - |  844 | `	 * so that we can consume the VM output and redirect it to STDOUT.` |
|     - |  845 | `	 */` |
|  5515 |  846 | `	rc = ph7_vm_config(pVm,` |
|     - |  847 | `		PH7_VM_CONFIG_OUTPUT,` |
|     - |  848 | `		Output_Consumer,    /* Output Consumer callback */` |
|     - |  849 | `		0                   /* Callback private data */` |
|     - |  850 | `		);` |
|  5515 |  851 | `	if( rc != PH7_OK ){` |
|   ! 0 |  852 | `		Fatal("Error while installing the VM output consumer callback");` |
|   ! 0 |  853 | `	}` |
|     - |  854 | `	/* Diagnostics stream: route the log copy of runtime warnings/notices and the` |
|     - |  855 | `	 * uncaught-exception fatal to STDERR (gated by log_errors), so program STDOUT` |
|     - |  856 | `	 * stays clean like stock CLI php. */` |
|  5515 |  857 | `	rc = ph7_vm_config(pVm,` |
|     - |  858 | `		PH7_VM_CONFIG_ERR_STREAM,` |
|     - |  859 | `		Error_Consumer,     /* Diagnostics (STDERR) consumer callback */` |
|     - |  860 | `		0                   /* Callback private data */` |
|     - |  861 | `		);` |
|  5515 |  862 | `	if( rc != PH7_OK ){` |
|   ! 0 |  863 | `		Fatal("Error while installing the VM diagnostics consumer callback");` |
|   ! 0 |  864 | `	}` |
|     - |  865 | `	/* Optional recursion caps via the environment (like PHL_MAX_ALLOC). The host` |
|     - |  866 | `	 * defaults are PHP-parity — PHP call depth is UNBOUNDED (heap-bound) and only` |
|     - |  867 | `	 * the native VmByteCodeExec nesting is capped — so these knobs are for tests` |
|     - |  868 | `	 * and embedders that want a tighter bound, not to raise a low default.` |
|     - |  869 | `	 *   PHL_MAX_RECURSION   -> PH7_VM_CONFIG_RECURSION_DEPTH (PHP call depth; any` |
|     - |  870 | `	 *                          positive value is a cap, PHL_EnvULong rejects 0)` |
|     - |  871 | `	 *   PHL_MAX_NATIVE_DEPTH -> PH7_VM_CONFIG_NATIVE_DEPTH   (native nesting;` |
|     - |  872 | `	 *                          floor 2) */` |
|     - |  873 | `	{` |
|     - |  874 | `		unsigned long uMax;` |
|  5515 |  875 | `		if( PHL_EnvULong("PHL_MAX_RECURSION",1UL,0x7FFFFFFFUL,&uMax) ){` |
|     5 |  876 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_RECURSION_DEPTH,(int)uMax);` |
|     2 |  877 | `		}` |
|  5515 |  878 | `		if( PHL_EnvULong("PHL_MAX_NATIVE_DEPTH",2UL,0x7FFFFFFFUL,&uMax) ){` |
|    12 |  879 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_NATIVE_DEPTH,(int)uMax);` |
|     5 |  880 | `		}` |
|     - |  881 | `	}` |
|     - |  882 | `	/* Define PHP_BINARY: absolute path of this interpreter */` |
|  8267 |  883 | `	ph7_create_constant(pVm,"PHP_BINARY",PHL_PhpBinaryConst,` |
|  5510 |  884 | `		(void *)PHL_ResolveBinaryPath(argv[0]));` |
|     - |  885 | `	/* Register the script arguments as $argv[] plus the matching $argc count and` |
|     - |  886 | `	 * the CLI $_SERVER entries, matching PHP: $argv[0] is the script path (file` |
|     - |  887 | `	 * mode) or the literal "Standard input code" (-r mode), followed by the` |
|     - |  888 | `	 * script's own arguments.` |
|     - |  889 | `	 */` |
|     - |  890 | `	{` |
|  5515 |  891 | `		const char *zScriptName = (run_code \|\| stdin_code) ? "Standard input code" : argv[n];` |
|  5515 |  892 | `		int argv_count = 0;` |
|     - |  893 | `		ph7_value *pArgc;` |
|     - |  894 | `		/* Count only the entries actually inserted, so $argc can never disagree` |
|     - |  895 | `		 * with count($argv) if a registration fails. */` |
|  5515 |  896 | `		if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,zScriptName) == PH7_OK ){` |
|  5512 |  897 | `			argv_count++;` |
|  2749 |  898 | `		}` |
|     - |  899 | `		/* The script's own arguments follow: in file mode argv[n] is the script` |
|     - |  900 | `		 * (registered above), so they start at n+1; in -r mode they start at n. */` |
|  5591 |  901 | `		for( n = (run_code \|\| stdin_code) ? n : n + 1; n < argc ; ++n ){` |
|    81 |  902 | `			if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,argv[n]) == PH7_OK ){` |
|    81 |  903 | `				argv_count++;` |
|    38 |  904 | `			}` |
|    43 |  905 | `		}` |
|     - |  906 | `		/* $argc: a plain integer global equal to count($argv). */` |
|  5515 |  907 | `		pArgc = ph7_new_scalar(pVm);` |
|  5515 |  908 | `		if( pArgc ){` |
|  5512 |  909 | `			ph7_value_int(pArgc,argv_count);` |
|  5512 |  910 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_CREATE_VAR,"argc",pArgc);` |
|  5512 |  911 | `			ph7_release_value(pVm,pArgc);` |
|  2749 |  912 | `		}` |
|     - |  913 | `		/* Mirror $argv/$argc into $_SERVER['argv']/$_SERVER['argc'] (php CLI). */` |
|  5515 |  914 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ARGV);` |
|     - |  915 | `		/* $_SERVER entries frameworks read at CLI bootstrap. SCRIPT_FILENAME is` |
|     - |  916 | `		 * already set to the script path by PH7_HashmapCreateSuper. */` |
|  5515 |  917 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"SCRIPT_NAME",zScriptName,-1);` |
|  5515 |  918 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PHP_SELF",zScriptName,-1);` |
|  5515 |  919 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"DOCUMENT_ROOT","",0);` |
|     - |  920 | `		{` |
|     - |  921 | `			char zTime[32];` |
|  5515 |  922 | `			snprintf(zTime,sizeof(zTime),"%ld",(long)time(0));` |
|  5515 |  923 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"REQUEST_TIME",zTime,-1);` |
|     - |  924 | `		}` |
|     - |  925 | `#ifndef __WINNT__` |
|     - |  926 | `		{` |
|     - |  927 | `			char zCwd[PATH_MAX];` |
|  5510 |  928 | `			if( getcwd(zCwd,sizeof(zCwd)) ){` |
|  5507 |  929 | `				ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PWD",zCwd,-1);` |
|  2749 |  930 | `			}` |
|     - |  931 | `		}` |
|     - |  932 | `#endif` |
|     - |  933 | `	}` |
|     - |  934 | `	/* Report script run-time errors (now default behavior) */` |
|  5515 |  935 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_ERR_REPORT);` |
|     - |  936 | `	/* Apply php.ini directives AFTER the error-report default so` |
|     - |  937 | ``	 * `-d error_reporting=0` can lower it: the -c file first, then -d`` |
|     - |  938 | `	 * overrides in CLI order (php's precedence). */` |
|  5515 |  939 | `	if( zIniFile ){` |
|     4 |  940 | `		PHL_LoadIniFile(pVm,zIniFile);` |
|     2 |  941 | `	}` |
|     - |  942 | `	{` |
|     - |  943 | `		int i;` |
|  5638 |  944 | `		for( i = 0 ; i < nIniDefine ; i++ ){` |
|   127 |  945 | `			PHL_ApplyIniPair(pVm,azIniDefine[i]);` |
|    65 |  946 | `		}` |
|     - |  947 | `	}` |
|  5515 |  948 | `	if( dump_vm ){` |
|     - |  949 | `		/* Dump PH7 byte-code instructions */` |
|     3 |  950 | `		ph7_vm_dump_v2(pVm,` |
|     - |  951 | `			Output_Consumer, /* Dump consumer callback */` |
|     - |  952 | `			0` |
|     - |  953 | `			);` |
|     1 |  954 | `	}` |
|     - |  955 | `	/*` |
|     - |  956 | `	 * And finally, execute our program. Note that your output (STDOUT in our case)` |
|     - |  957 | `	 * should display the result.` |
|     - |  958 | `	 */` |
|     - |  959 | `	{` |
|  5515 |  960 | `		int iExitStatus = 0;` |
|  5515 |  961 | `		ph7_vm_exec(pVm,&iExitStatus);` |
|     - |  962 | `		/* All done, cleanup the mess left behind.` |
|     - |  963 | `		*/` |
|  5523 |  964 | `		ph7_vm_release(pVm);` |
|  5523 |  965 | `		ph7_release(pEngine);` |
|  5523 |  966 | `		pFatalEngine = 0;` |
|     - |  967 | `		/* The stdin slurp outlives compilation (the compiler keeps pointers` |
|     - |  968 | `		 * into the source text), so it is freed only here, after the VM. */` |
|  5523 |  969 | `		if( zStdinCode ){` |
|    12 |  970 | `			free(zStdinCode);` |
|     6 |  971 | `		}` |
|     - |  972 | `		/* Propagate the script exit status (set via exit()/die()) */` |
|  5523 |  973 | `		return iExitStatus;` |
|     - |  974 | `	}` |
|  2896 |  975 | `}` |
|     - |  976 |  |
