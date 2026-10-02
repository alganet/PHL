# src/phl/phl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 719/885 lines (81.24%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `/*` |
|      - |    7 | ` * The PHL interpreter is a simple stand-alone PHP interpreter that allows` |
|      - |    8 | ` * the user to enter and execute PHP files against a PH7 engine.` |
|      - |    9 | ` * To start the phl program, just type "phl" followed by the name of the PHP file` |
|      - |   10 | ` * to compile and execute. That is, the first argument is to the interpreter, the rest` |
|      - |   11 | ` * are scripts arguments, press "Enter" and the PHP code will be executed.` |
|      - |   12 | ` * If something goes wrong while processing the PHP script due to a compile-time error` |
|      - |   13 | ` * your error output (STDOUT) should display the compile-time error messages.` |
|      - |   14 | ` *` |
|      - |   15 | ` * Usage example of the phl interpreter:` |
|      - |   16 | ` *   phl hello_world.php` |
|      - |   17 | ` * Running the interpreter with script arguments` |
|      - |   18 | ` *    phl scripts/mp3_tag.php /usr/local/path/to/my_mp3s` |
|      - |   19 | ` *` |
|      - |   20 | ` * Command line options:` |
|      - |   21 | ` *   -b: Dump PH7 byte-code instructions` |
|      - |   22 | ` *   -h: Display this help message` |
|      - |   23 | ` *` |
|      - |   24 | ` * The PHL interpreter package includes more than 70 PHP scripts to test ranging from` |
|      - |   25 | ` * simple hello world programs to XML processing, ZIP archive extracting, MP3 tag extracting,` |
|      - |   26 | ` * UUID generation, JSON encoding/decoding, INI processing, Base32 encoding/decoding and many` |
|      - |   27 | ` * more. These scripts are available in the scripts directory from the zip archive.` |
|      - |   28 | ` */` |
|      - |   29 | `#include <stdio.h>` |
|      - |   30 | `#include <stdlib.h>` |
|      - |   31 | `#include <string.h>` |
|      - |   32 | `#include <time.h>` |
|      - |   33 | `#include <errno.h>` |
|      - |   34 | `#include <locale.h>` |
|      - |   35 | `#ifndef __WINNT__` |
|      - |   36 | `#include <signal.h>   /* SIGPIPE — see main() */` |
|      - |   37 | `#endif` |
|      - |   38 | `#ifdef __UNIXES__` |
|      - |   39 | `#include <unistd.h>` |
|      - |   40 | `#include <sys/stat.h>  /* the -c door has to recognise a directory -- see PHL_OpenIniCandidate */` |
|      - |   41 | `#endif` |
|      - |   42 | `/* Make sure this header file is available.*/` |
|      - |   43 | `#include "ph7.h"` |
|      - |   44 | `#ifdef PHL_ENABLE_SERVER` |
|      - |   45 | `#include "server.h"` |
|      - |   46 | `#endif` |
|      - |   47 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|      - |   48 | `#define MINIDUMP_IMPLEMENTATION` |
|      - |   49 | `#include "minidump.h"` |
|      - |   50 | `#endif` |
|      - |   51 | `/*` |
|      - |   52 | ` * Display an error message and exit.` |
|      - |   53 | ` */` |
|    ! 0 |   54 | `static void FatalCode(const char *zMsg,int iCode)` |
|    ! 0 |   55 | `{` |
|    ! 0 |   56 | `	puts(zMsg);` |
|      - |   57 | `	/* Shutdown the library */` |
|    ! 0 |   58 | `	ph7_lib_shutdown();` |
|      - |   59 | `	/* Exit immediately */` |
|    ! 0 |   60 | `	exit(iCode);` |
|    ! 0 |   61 | `}` |
|      - |   62 | `/*` |
|      - |   63 | ` * php-parity default: fatal engine/compile failures exit 255 (php exits 255` |
|      - |   64 | ` * on a fatal compile error); usage and IO errors use FatalCode(msg, 1)` |
|      - |   65 | ` * directly, mirroring php's exit 1 for bad invocations / unopenable input.` |
|      - |   66 | ` */` |
|    ! 0 |   67 | `static void Fatal(const char *zMsg)` |
|    ! 0 |   68 | `{` |
|    ! 0 |   69 | `	FatalCode(zMsg,255);` |
|    ! 0 |   70 | `}` |
|      - |   71 | `/*` |
|      - |   72 | ` * Exit 255 without printing anything: used when the diagnostic has already been` |
|      - |   73 | ` * emitted by the error consumer (a compile/parse error), which is exactly what` |
|      - |   74 | ` * php does — it prints the parse error and nothing more.` |
|      - |   75 | ` */` |
|      - |   76 | `/* The engine this process built, so the silent-fatal exit can give it back:` |
|      - |   77 | ` * ph7_lib_shutdown() releases the LIBRARY, and an engine instance owns a mutex` |
|      - |   78 | ` * of its own. Without this the fatal path leaks it, which is invisible in` |
|      - |   79 | ` * ordinary use (the process is exiting) and turns every leak-detecting run over` |
|      - |   80 | ` * a corpus that spawns a failing child into a wall of reports. */` |
|      - |   81 | `static ph7 *pFatalEngine = 0;` |
|   1020 |   82 | `static void FatalSilentCode(int iCode)` |
|      4 |   83 | `{` |
|   1024 |   84 | `	if( pFatalEngine ){` |
|   1024 |   85 | `		ph7_release(pFatalEngine);` |
|   1024 |   86 | `		pFatalEngine = 0;` |
|    510 |   87 | `	}` |
|   1024 |   88 | `	ph7_lib_shutdown();` |
|   1024 |   89 | `	exit(iCode);` |
|    ! 0 |   90 | `}` |
|   1016 |   91 | `static void FatalSilent(void)` |
|      4 |   92 | `{` |
|   1020 |   93 | `	FatalSilentCode(255);` |
|    508 |   94 | `}` |
|      - |   95 | `/*` |
|      - |   96 | ` * Display the banner,a help message and exit.` |
|      - |   97 | ` */` |
|      2 |   98 | `static void Help(void)` |
|      1 |   99 | `{` |
|      3 |  100 | `	puts("phl [-h\|--help\|-b\|-i\|-l\|-v\|--version\|-r code\|--rf name\|--rc name\|--re name\|-d name=value\|-c inifile] path/to/php_file [script args]");` |
|      - |  101 | `#ifdef PHL_ENABLE_SERVER` |
|      3 |  102 | `	puts("phl -S host:port [-t docroot] [router.php]");` |
|      - |  103 | `#endif` |
|      3 |  104 | `	puts("\t-b: Dump PH7 byte-code instructions");` |
|      3 |  105 | `	puts("\t-i: Display interpreter information and exit");` |
|      3 |  106 | `	puts("\t-l: Syntax-check (lint) the given file and exit");` |
|      3 |  107 | `	puts("\t-r code: Run code from command line (no tags needed)");` |
|      - |  108 | `#ifdef PHL_ENABLE_SERVER` |
|      3 |  109 | `	puts("\t-S host:port: Start the built-in development server");` |
|      3 |  110 | `	puts("\t-t docroot: Document root for the server (default: current directory)");` |
|      - |  111 | `#endif` |
|      3 |  112 | `	puts("\t-v, --version: Display version information and exit");` |
|      3 |  113 | `	puts("\t-h, --help: Display this message and exit");` |
|      - |  114 | `	/* Exit immediately */` |
|      3 |  115 | `	exit(0);` |
|    ! 0 |  116 | `}` |
|      - |  117 | `/*` |
|      - |  118 | ` * Display version information and exit.` |
|      - |  119 | ` */` |
|      6 |  120 | `static void Version(void)` |
|      1 |  121 | `{` |
|      7 |  122 | `	puts("PHL " PH7_VERSION " (cli) (built " __DATE__ " " __TIME__ ")");` |
|      7 |  123 | `	puts("Copyright (c) 2011-2014 Symisc Systems, 2025 Alexandre Gomes Gaigalas");` |
|      - |  124 | `	/* Exit immediately */` |
|      7 |  125 | `	exit(0);` |
|    ! 0 |  126 | `}` |
|      - |  127 | `/*` |
|      - |  128 | ` * Display interpreter information (php -i) and exit. PHP's CLI -i is plain text` |
|      - |  129 | ` * (the phpinfo() builtin emits HTML, suited to the web SAPI), so this prints a` |
|      - |  130 | ` * concise curated subset on the terminal rather than reusing that builtin.` |
|      - |  131 | ` */` |
|      2 |  132 | `static void Info(void)` |
|    ! 0 |  133 | `{` |
|      2 |  134 | `	printf("phpinfo()\n");` |
|      2 |  135 | `	printf("PHP Version => %s\n\n", PHP_COMPAT_VERSION);` |
|      2 |  136 | `	printf("System => %s\n",` |
|      - |  137 | `#ifdef __WINNT__` |
|      - |  138 | `		"Windows NT"` |
|      - |  139 | `#elif defined(__UNIXES__)` |
|      - |  140 | `		"UNIX-Like"` |
|      - |  141 | `#else` |
|      - |  142 | `		"Other OS"` |
|      - |  143 | `#endif` |
|      - |  144 | `	);` |
|      2 |  145 | `	printf("Build Date => %s %s\n", __DATE__, __TIME__);` |
|      2 |  146 | `	printf("PHL Version => %s\n", PH7_VERSION);` |
|      2 |  147 | `	printf("PHP SAPI => cli\n");` |
|      - |  148 | `	/* Exit immediately */` |
|      2 |  149 | `	exit(0);` |
|    ! 0 |  150 | `}` |
|      - |  151 | `#ifdef __WINNT__` |
|      - |  152 | `#include <Windows.h>` |
|      - |  153 | `#else` |
|      - |  154 | `/* Assume UNIX */` |
|      - |  155 | `#include <unistd.h>` |
|      - |  156 | `#include <limits.h>` |
|      - |  157 | `#endif` |
|      - |  158 | `/*` |
|      - |  159 | ` * The following define is used by the UNIX built and have` |
|      - |  160 | ` * no particular meaning on windows.` |
|      - |  161 | ` */` |
|      - |  162 | `#ifndef STDOUT_FILENO` |
|      - |  163 | `#define STDOUT_FILENO	1` |
|      - |  164 | `#endif` |
|      - |  165 | `#ifndef STDERR_FILENO` |
|      - |  166 | `#define STDERR_FILENO	2` |
|      - |  167 | `#endif` |
|      - |  168 | `#ifndef PATH_MAX` |
|      - |  169 | `#define PATH_MAX 4096` |
|      - |  170 | `#endif` |
|      - |  171 | `static char zPhlBinaryPath[PATH_MAX];` |
|      - |  172 | `/*` |
|      - |  173 | ` * Expand callback for the PHP_BINARY constant.` |
|      - |  174 | ` * pUserData points to the resolved binary path.` |
|      - |  175 | ` */` |
|    695 |  176 | `static void PHL_PhpBinaryConst(ph7_value *pVal,void *pUserData)` |
|      5 |  177 | `{` |
|    700 |  178 | `	ph7_value_string(pVal,(const char *)pUserData,-1);` |
|    700 |  179 | `}` |
|      - |  180 | `/*` |
|      - |  181 | ` * Resolve the absolute path of the running interpreter.` |
|      - |  182 | ` * Falls back to argv[0] verbatim (e.g. bare PATH invocation):` |
|      - |  183 | ` * consumers spawning it again go through the shell, which re-resolves it.` |
|      - |  184 | ` */` |
|   6561 |  185 | `static const char * PHL_ResolveBinaryPath(const char *zArgv0)` |
|      5 |  186 | `{` |
|      - |  187 | `#ifdef __WINNT__` |
|      5 |  188 | `	DWORD nLen = GetModuleFileNameA(0,zPhlBinaryPath,(DWORD)sizeof(zPhlBinaryPath));` |
|      5 |  189 | `	if( nLen > 0 && nLen < sizeof(zPhlBinaryPath) ){` |
|      5 |  190 | `		return zPhlBinaryPath;` |
|      - |  191 | `	}` |
|      - |  192 | `#else` |
|   6561 |  193 | `	if( realpath(zArgv0,zPhlBinaryPath) != 0 ){` |
|   6561 |  194 | `		return zPhlBinaryPath;` |
|      - |  195 | `	}` |
|      - |  196 | `#endif` |
|    ! 0 |  197 | `	return zArgv0;` |
|   3280 |  198 | `}` |
|      - |  199 | `/*` |
|      - |  200 | ` * VM output consumer callback.` |
|      - |  201 | ` * Each time the virtual machine generates some outputs,the following` |
|      - |  202 | ` * function gets called by the underlying virtual machine to consume` |
|      - |  203 | ` * the generated output.` |
|      - |  204 | ` * All this function does is redirecting the VM output to STDOUT.` |
|      - |  205 | ` * This function is registered later via a call to ph7_vm_config()` |
|      - |  206 | ` * with a configuration verb set to: PH7_VM_CONFIG_OUTPUT.` |
|      - |  207 | ` */` |
| 101814 |  208 | `static int Output_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)` |
|      5 |  209 | `{` |
|  49942 |  210 | `	(void)pUserData;` |
|      - |  211 | `#ifdef __WINNT__` |
|      - |  212 | `	BOOL rc;` |
|      5 |  213 | `	rc = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),pOutput,(DWORD)nOutputLen,0,0);` |
|      5 |  214 | `	if( !rc ){` |
|      - |  215 | `		/* Abort processing */` |
|    ! 0 |  216 | `		return PH7_ABORT;` |
|      - |  217 | `	}` |
|      - |  218 | `#else` |
|      - |  219 | `	ssize_t nWr;` |
| 101814 |  220 | `	nWr = write(STDOUT_FILENO,pOutput,nOutputLen);` |
| 101814 |  221 | `	if( nWr < 0 ){` |
|      - |  222 | `		/* Abort processing */` |
|    ! 0 |  223 | `		return PH7_ABORT;` |
|      - |  224 | `	}` |
|      - |  225 | `#endif /* __WINT__ */` |
|      - |  226 | `	/* All done,VM output was redirected to STDOUT */` |
| 101819 |  227 | `	return PH7_OK;` |
|  49947 |  228 | `}` |
|      - |  229 | `/*` |
|      - |  230 | ` * VM diagnostics consumer (PH7_VM_CONFIG_ERR_STREAM): the log copy of a runtime` |
|      - |  231 | ` * warning/notice/deprecation and the uncaught-exception fatal go here — STDERR —` |
|      - |  232 | ` * so program STDOUT stays clean, matching stock CLI php.` |
|      - |  233 | ` */` |
|   2634 |  234 | `static int Error_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)` |
|      5 |  235 | `{` |
|   1317 |  236 | `	(void)pUserData;` |
|      - |  237 | `#ifdef __WINNT__` |
|      - |  238 | `	BOOL rc;` |
|      5 |  239 | `	rc = WriteFile(GetStdHandle(STD_ERROR_HANDLE),pOutput,(DWORD)nOutputLen,0,0);` |
|      5 |  240 | `	if( !rc ){` |
|      - |  241 | `		/* Abort processing */` |
|    ! 0 |  242 | `		return PH7_ABORT;` |
|      - |  243 | `	}` |
|      - |  244 | `#else` |
|      - |  245 | `	ssize_t nWr;` |
|   2634 |  246 | `	nWr = write(STDERR_FILENO,pOutput,nOutputLen);` |
|   2634 |  247 | `	if( nWr < 0 ){` |
|      - |  248 | `		/* Abort processing */` |
|    ! 0 |  249 | `		return PH7_ABORT;` |
|      - |  250 | `	}` |
|      - |  251 | `#endif /* __WINNT__ */` |
|      - |  252 | `	/* All done, VM diagnostics were redirected to STDERR */` |
|   2639 |  253 | `	return PH7_OK;` |
|   1322 |  254 | `}` |
|      - |  255 | `/*` |
|      - |  256 | ` * Parse an unsigned-long testing knob from the environment (PHL_MAX_ALLOC /` |
|      - |  257 | ` * PHL_MAX_INPUT / PHL_MAX_RECURSION / PHL_MAX_NATIVE_DEPTH). Returns 1 and writes` |
|      - |  258 | ` * *pOut on a valid, strictly-positive, fully-numeric value clamped to` |
|      - |  259 | ` * [uFloor, uCeil]; returns` |
|      - |  260 | ` * 0 (leaving *pOut untouched) when the var is unset, empty, non-numeric, has` |
|      - |  261 | ` * trailing garbage, or is zero — so a typo like "-1" or "abc" is ignored` |
|      - |  262 | ` * rather than silently reinterpreted (strtoul would wrap "-1" to ULONG_MAX).` |
|      - |  263 | ` */` |
|  28864 |  264 | `static int PHL_EnvULong(const char *zName,unsigned long uFloor,unsigned long uCeil,unsigned long *pOut)` |
|      5 |  265 | `{` |
|  28869 |  266 | `	const char *zVal = getenv(zName);` |
|  28869 |  267 | `	char *zEnd = 0;` |
|      - |  268 | `	unsigned long uMax;` |
|  28869 |  269 | `	if( zVal == 0 \|\| zVal[0] == 0 ){` |
|  28855 |  270 | `		return 0;` |
|      - |  271 | `	}` |
|      - |  272 | `	/* Reject a leading sign outright: strtoul silently negates "-1" to` |
|      - |  273 | `	 * ULONG_MAX, turning a typo into an effectively-unlimited cap. */` |
|     17 |  274 | `	if( zVal[0] == '-' \|\| zVal[0] == '+' ){` |
|    ! 0 |  275 | `		return 0;` |
|      - |  276 | `	}` |
|     17 |  277 | `	errno = 0;` |
|     17 |  278 | `	uMax = strtoul(zVal,&zEnd,10);` |
|     17 |  279 | `	if( errno != 0 \|\| zEnd == zVal \|\| *zEnd != 0 \|\| uMax == 0 ){` |
|    ! 0 |  280 | `		return 0; /* non-numeric, trailing junk, overflow, or zero */` |
|      - |  281 | `	}` |
|     17 |  282 | `	if( uMax < uFloor ){` |
|    ! 0 |  283 | `		uMax = uFloor;` |
|    ! 0 |  284 | `	}` |
|     17 |  285 | `	if( uMax > uCeil ){` |
|    ! 0 |  286 | `		uMax = uCeil;` |
|    ! 0 |  287 | `	}` |
|     17 |  288 | `	*pOut = uMax;` |
|     17 |  289 | `	return 1;` |
|  14415 |  290 | `}` |
|      - |  291 | `/*` |
|      - |  292 | ` * php's ini scanner is not line-oriented, and a php.ini VALUE is not a line.` |
|      - |  293 | ` * A quoted run is a scanner STATE that keeps going past the newline, so where` |
|      - |  294 | ` * one directive's value ends -- and what line the next directive is on -- are` |
|      - |  295 | ` * properties of the whole SOURCE:` |
|      - |  296 | ` *` |
|      - |  297 | `` *   . `x = 'a<NL>b'` is one raw string carrying a newline, and the rest of the`` |
|      - |  298 | `` *     file parses normally behind it. Reading a line at a time stored `'a` and`` |
|      - |  299 | ` *     called it a syntax error.` |
|      - |  300 | ` *   . A quote with no partner runs to the end of the source and takes every` |
|      - |  301 | ` *     directive behind it down with it -- silently when the value in front of` |
|      - |  302 | `` *     it had already reduced (`x = (1)'`), as php's own parser does.`` |
|      - |  303 | ` *   . Only the double-quoted run moves the line counter over the newlines it` |
|      - |  304 | ` *     ate; php's raw-string rule is a single regex match that never touches it.` |
|      - |  305 | `` *     So a refusal below `x = "a<NL>b"` is dated one line lower than the same`` |
|      - |  306 | `` *     one below `x = 'a<NL>b'`.`` |
|      - |  307 | ` *` |
|      - |  308 | ` * So walk the source once and hand the engine each directive with the line it` |
|      - |  309 | ` * was written on. The value grammar itself lives in vm.c (VmIniEvalValue) and is` |
|      - |  310 | ` * handed exactly the bytes php's scanner would have given its parser: the value` |
|      - |  311 | ` * text with its quoted runs intact, without the comment or the newline that` |
|      - |  312 | ` * closes the directive, and running to the end of the source when a quote never` |
|      - |  313 | ` * closes.` |
|      - |  314 | ` */` |
|   1385 |  315 | `static const char * PHL_IniSkipEol(const char *z,const char *zEnd)` |
|      4 |  316 | `{` |
|   1523 |  317 | `	while( z < zEnd && z[0] != '\n' && z[0] != '\r' ){` |
|    134 |  318 | `		z++;` |
|    ! 0 |  319 | `	}` |
|   1389 |  320 | `	return z;` |
|      4 |  321 | `}` |
|   1487 |  322 | `static const char * PHL_IniEatEol(const char *z,const char *zEnd)` |
|      4 |  323 | `{` |
|   1491 |  324 | `	if( z < zEnd && z[0] == '\r' ){` |
|      2 |  325 | `		z++;` |
|      1 |  326 | `	}` |
|   1491 |  327 | `	if( z < zEnd && z[0] == '\n' ){` |
|   1491 |  328 | `		z++;` |
|    743 |  329 | `	}` |
|   1491 |  330 | `	return z;` |
|      4 |  331 | `}` |
|      - |  332 | `/*` |
|      - |  333 | ` * Hand one directive over. The value's TEXT is what travels: the engine runs` |
|      - |  334 | `` * php's ini value grammar over it, which is what turns `E_ALL & ~E_NOTICE` into`` |
|      - |  335 | `` * a number, `On` into "1" and a quoted run into its literal bytes -- so`` |
|      - |  336 | ` * unquoting here would hand that grammar a constant name where php hands it` |
|      - |  337 | ` * four letters.` |
|      - |  338 | ` */` |
|   1369 |  339 | `static void PHL_ApplyIniValue(ph7 *pEngine,const char *zName,size_t nName,` |
|      - |  340 | `	const char *zVal,size_t nVal,const char *zFile,unsigned int nLine,int iStop)` |
|      4 |  341 | `{` |
|      - |  342 | `	char zNameBuf[128];` |
|      - |  343 | `	char zStack[512];` |
|   1373 |  344 | `	char *zValue = zStack;   /* heap-backed past what the stack buffer holds */` |
|   1373 |  345 | `	if( nName == 0 \|\| nName >= sizeof(zNameBuf) ){` |
|      2 |  346 | `		return;` |
|      - |  347 | `	}` |
|   1371 |  348 | `	memcpy(zNameBuf,zName,nName);` |
|   1371 |  349 | `	zNameBuf[nName] = 0;` |
|   1371 |  350 | `	if( nVal + 1 > sizeof(zStack) ){` |
|      - |  351 | `		/* php has no ceiling on an ini value, and a real one can be long:` |
|      - |  352 | `		 * PHPUnit hands its child a ~700-byte redaction pattern. Truncating` |
|      - |  353 | `		 * at a fixed width stored half a value, and cutting one in the` |
|      - |  354 | `		 * middle of a quoted run made the whole directive a syntax error. */` |
|      3 |  355 | `		zValue = (char *)malloc(nVal + 1);` |
|      3 |  356 | `		if( zValue == 0 ){` |
|    ! 0 |  357 | `			return;` |
|      - |  358 | `		}` |
|      1 |  359 | `	}` |
|   1371 |  360 | `	if( nVal > 0 ){` |
|   1367 |  361 | `		memcpy(zValue,zVal,nVal);` |
|    681 |  362 | `	}` |
|   1371 |  363 | `	zValue[nVal] = 0;` |
|   1371 |  364 | `	ph7_config(pEngine,PH7_CONFIG_INI_ENTRY,zNameBuf,zValue,zFile,nLine,iStop);` |
|   1371 |  365 | `	if( zValue != zStack ){` |
|      3 |  366 | `		free(zValue);` |
|      1 |  367 | `	}` |
|    688 |  368 | `}` |
|      - |  369 | `/*` |
|      - |  370 | `` * Hand over a `[` php's scanner never closes. It stands in the queue where a`` |
|      - |  371 | ` * directive would, carries no name, and takes the whole source down with it.` |
|      - |  372 | ` */` |
|     62 |  373 | `static void PHL_RefuseIniSource(ph7 *pEngine,const char *zFile,unsigned int nLine,int iStop)` |
|    ! 0 |  374 | `{` |
|     62 |  375 | `	ph7_config(pEngine,PH7_CONFIG_INI_ENTRY,"","",zFile,nLine,iStop);` |
|     62 |  376 | `}` |
|      - |  377 | `/*` |
|      - |  378 | ` * Hand over the text standing where php's scanner reads a directive NAME,` |
|      - |  379 | ` * ahead of whatever is made of it. php's INITIAL is not "everything up to the` |
|      - |  380 | `` * `=`": it has a rule for each of its bool words in front of the one that`` |
|      - |  381 | ` * reads a LABEL, and a dozen bytes of its punctuation are tokens no statement` |
|      - |  382 | `` * of its grammar starts with -- so `on = 1` and `x]y = 1` refuse the source`` |
|      - |  383 | `` * from here down where `onx = 1` and `x.y = 1` are ordinary entries. The`` |
|      - |  384 | ` * engine owns that table, next to the value grammar that shares its bool` |
|      - |  385 | ` * words; a second copy of it out here would drift away from it.` |
|      - |  386 | ` */` |
|   1447 |  387 | `static void PHL_ScreenIniStmt(ph7 *pEngine,const char *zStmt,size_t nStmt,` |
|      - |  388 | `	const char *zFile,unsigned int nLine)` |
|      4 |  389 | `{` |
|      - |  390 | `	char zStack[512];` |
|   1451 |  391 | `	char *zText = zStack;` |
|   1451 |  392 | `	if( nStmt + 1 > sizeof(zStack) ){` |
|    ! 0 |  393 | `		zText = (char *)malloc(nStmt + 1);` |
|    ! 0 |  394 | `		if( zText == 0 ){` |
|    ! 0 |  395 | `			return;` |
|      - |  396 | `		}` |
|    ! 0 |  397 | `	}` |
|   1451 |  398 | `	if( nStmt > 0 ){` |
|   1451 |  399 | `		memcpy(zText,zStmt,nStmt);` |
|    723 |  400 | `	}` |
|   1451 |  401 | `	zText[nStmt] = 0;` |
|   1451 |  402 | `	ph7_config(pEngine,PH7_CONFIG_INI_ENTRY,zText,"",zFile,nLine,PH7_INI_STOP_STMT);` |
|   1451 |  403 | `	if( zText != zStack ){` |
|    ! 0 |  404 | `		free(zText);` |
|    ! 0 |  405 | `	}` |
|    727 |  406 | `}` |
|      - |  407 | `/*` |
|      - |  408 | `` * Hand over a `${` run found inside a section NAME, from its `$` to the end of`` |
|      - |  409 | ` * the source. php's ST_VARNAME and ST_VAR_FALLBACK are reached from its section` |
|      - |  410 | ` * state and from its value state alike -- one run, one grammar, one set of` |
|      - |  411 | ` * refusals -- so the screen is the engine's own variable rule rather than a` |
|      - |  412 | ` * second reading of it out here. The engine answers nothing at all when php` |
|      - |  413 | ` * reads the substitution, and the source's refusal when it does not; either way` |
|      - |  414 | ` * the entry stands in the queue ahead of everything the header is followed by,` |
|      - |  415 | `` * so a refused `${` takes those down the way any other ini error does.`` |
|      - |  416 | ` */` |
|     52 |  417 | `static void PHL_ScreenIniVar(ph7 *pEngine,const char *zRun,size_t nRun,` |
|      - |  418 | `	const char *zFile,unsigned int nLine)` |
|    ! 0 |  419 | `{` |
|      - |  420 | `	char zStack[512];` |
|     52 |  421 | `	char *zText = zStack;` |
|     52 |  422 | `	if( nRun + 1 > sizeof(zStack) ){` |
|    ! 0 |  423 | `		zText = (char *)malloc(nRun + 1);` |
|    ! 0 |  424 | `		if( zText == 0 ){` |
|    ! 0 |  425 | `			return;` |
|      - |  426 | `		}` |
|    ! 0 |  427 | `	}` |
|     52 |  428 | `	if( nRun > 0 ){` |
|     52 |  429 | `		memcpy(zText,zRun,nRun);` |
|     26 |  430 | `	}` |
|     52 |  431 | `	zText[nRun] = 0;` |
|     52 |  432 | `	ph7_config(pEngine,PH7_CONFIG_INI_ENTRY,zText,"",zFile,nLine,` |
|      - |  433 | `		PH7_INI_STOP_SECTION_VAR);` |
|     52 |  434 | `	if( zText != zStack ){` |
|    ! 0 |  435 | `		free(zText);` |
|    ! 0 |  436 | `	}` |
|     26 |  437 | `}` |
|      - |  438 | `/*` |
|      - |  439 | `` * How far one `${` run reaches, with z on its `$`. Answers the byte behind the`` |
|      - |  440 | `` * `}` that closes it, or 0 when nothing does. Only the EXTENT is decided here:`` |
|      - |  441 | ` * whether php reads the run at all is the engine's answer (PHL_ScreenIniVar),` |
|      - |  442 | ` * and where the two readings could differ the engine has already refused the` |
|      - |  443 | `` * source -- so the walk only has to land on the same `}` for a run php accepts.`` |
|      - |  444 | ` * That is the name run, which stops at a byte its LABEL cannot hold and carries` |
|      - |  445 | `` * no escapes, and then the fallback behind a `:-`, which keeps a `\<byte>` pair`` |
|      - |  446 | `` * whole, takes a nested `${}` and a double-quoted run, and ends at anything`` |
|      - |  447 | ` * else. Only the double-quoted run moves *pnLine, exactly as it does in a value.` |
|      - |  448 | ` */` |
|     56 |  449 | `static const char * PHL_IniVarRun(const char *z,const char *zEnd,unsigned int *pnLine)` |
|    ! 0 |  450 | `{` |
|     56 |  451 | ``	z += 2;   /* `${` */`` |
|      - |  452 | `	/* The name run's own end is the engine's table, but a run php ACCEPTS can` |
|      - |  453 | ``	 * only end on the `}` that closes it or on the `:-` that opens a fallback:`` |
|      - |  454 | `	 * every other byte that ends a LABEL is then standing where the grammar` |
|      - |  455 | `	 * wants one of those two, which is a refusal the engine has already named.` |
|      - |  456 | `	 * So the walk needs neither the table nor a copy of it. */` |
|    232 |  457 | `	while( z < zEnd && z[0] != '}' ){` |
|    208 |  458 | `		if( z[0] == ':' && &z[1] < zEnd && z[1] == '-' ){` |
|     32 |  459 | `			break;` |
|      - |  460 | `		}` |
|    176 |  461 | `		z++;` |
|    ! 0 |  462 | `	}` |
|     56 |  463 | `	if( z < zEnd && z[0] == ':' && &z[1] < zEnd && z[1] == '-' ){` |
|     32 |  464 | `		z += 2;` |
|     72 |  465 | `		while( z < zEnd ){` |
|     72 |  466 | `			int c = (unsigned char)z[0];` |
|     72 |  467 | `			if( c == '}' ){` |
|     18 |  468 | `				break;` |
|      - |  469 | `			}` |
|     54 |  470 | `			if( c == '\\' && &z[1] < zEnd ){` |
|      2 |  471 | `				z += 2;` |
|      2 |  472 | `				continue;` |
|      - |  473 | `			}` |
|     52 |  474 | `			if( c == '$' && &z[1] < zEnd && z[1] == '{' ){` |
|      4 |  475 | `				z = PHL_IniVarRun(z,zEnd,pnLine);` |
|      4 |  476 | `				if( z == 0 ){` |
|    ! 0 |  477 | `					return 0;` |
|      - |  478 | `				}` |
|      4 |  479 | `				continue;` |
|      - |  480 | `			}` |
|     48 |  481 | `			if( c == '"' ){` |
|      8 |  482 | `				const char *zQ = &z[1];` |
|     78 |  483 | `				while( zQ < zEnd && zQ[0] != '"' ){` |
|     70 |  484 | `					if( zQ[0] == '\\' && &zQ[1] < zEnd ){` |
|    ! 0 |  485 | `						zQ += 2;` |
|    ! 0 |  486 | `						continue;` |
|      - |  487 | `					}` |
|     70 |  488 | `					if( zQ[0] == '\n' \|\| zQ[0] == '\r' ){` |
|     10 |  489 | `						zQ = PHL_IniEatEol(zQ,zEnd);` |
|     10 |  490 | `						(*pnLine)++;` |
|     10 |  491 | `						continue;` |
|      - |  492 | `					}` |
|     60 |  493 | `					zQ++;` |
|    ! 0 |  494 | `				}` |
|      8 |  495 | `				if( zQ >= zEnd ){` |
|      4 |  496 | `					return 0;` |
|      - |  497 | `				}` |
|      4 |  498 | `				z = &zQ[1];` |
|      4 |  499 | `				continue;` |
|      - |  500 | `			}` |
|     40 |  501 | `			if( c == '\n' \|\| c == '\r' \|\| c == ';' \|\| c == '\'' ){` |
|      5 |  502 | `				break;` |
|      - |  503 | `			}` |
|     30 |  504 | `			z++;` |
|    ! 0 |  505 | `		}` |
|     14 |  506 | `	}` |
|     52 |  507 | `	if( z >= zEnd \|\| z[0] != '}' ){` |
|     16 |  508 | `		return 0;` |
|      - |  509 | `	}` |
|     36 |  510 | `	return &z[1];` |
|     28 |  511 | `}` |
|      - |  512 | `/*` |
|      - |  513 | `` * php's ST_SECTION_VALUE and ST_OFFSET are one run, and `[s]` and `a[s]` are`` |
|      - |  514 | ` * the two statements that open it: SECTION_VALUE_CHARS, a backslash carrying` |
|      - |  515 | `` * whatever byte is behind it (a newline included), a raw `'` run, a`` |
|      - |  516 | `` * double-quoted run and a `${}` -- closed by `]` and by nothing else. A newline`` |
|      - |  517 | `` * does not close it and neither does a `;`: both leave the scanner with no rule`` |
|      - |  518 | `` * to match at all, which is php's end of INPUT, so a `[` that never meets its`` |
|      - |  519 | `` * bracket refuses the whole source from there down and a `]` written further`` |
|      - |  520 | ` * down does not rescue it.` |
|      - |  521 | ` *` |
|      - |  522 | `` * Answers the byte behind the `]`, or 0 with *piBad naming the refusal. Only a`` |
|      - |  523 | ` * double-quoted run moves *pnLine -- php's raw-string rule is one regex match` |
|      - |  524 | ` * that never touches the counter -- and it moves it whether or not the run is` |
|      - |  525 | ` * the thing that fails, so a refusal below a name carrying a quoted newline is` |
|      - |  526 | ` * dated where the counter got to.` |
|      - |  527 | ` *` |
|      - |  528 | `` * A `${` inside the name is handed to the engine as it is walked over, in the`` |
|      - |  529 | ` * order the names hold them: php's substitution is one grammar wherever it is` |
|      - |  530 | ` * written, and reading it out here instead would be a second copy of it.` |
|      - |  531 | ` */` |
|    170 |  532 | `static const char * PHL_IniBracketRun(ph7 *pEngine,const char *zFile,` |
|      - |  533 | `	const char *z,const char *zEnd,unsigned int *pnLine,int *piBad)` |
|    ! 0 |  534 | `{` |
|    170 |  535 | `	*piBad = PH7_INI_STOP_SECTION;` |
|    510 |  536 | `	while( z < zEnd ){` |
|    508 |  537 | `		int c = (unsigned char)z[0];` |
|    508 |  538 | `		if( c == ']' ){` |
|    108 |  539 | `			return &z[1];` |
|      - |  540 | `		}` |
|    400 |  541 | `		if( c == '\n' \|\| c == '\r' \|\| c == ';' ){` |
|     32 |  542 | `			return 0;   /* out of rules, and the source with it */` |
|      - |  543 | `		}` |
|    368 |  544 | `		if( c == '"' ){` |
|     12 |  545 | `			const char *zQ = &z[1];` |
|     84 |  546 | `			while( zQ < zEnd && zQ[0] != '"' ){` |
|     72 |  547 | `				if( zQ[0] == '\\' && &zQ[1] < zEnd ){` |
|    ! 0 |  548 | `					zQ += 2;` |
|    ! 0 |  549 | `					continue;` |
|      - |  550 | `				}` |
|     72 |  551 | `				if( zQ[0] == '\n' \|\| zQ[0] == '\r' ){` |
|     14 |  552 | `					zQ = PHL_IniEatEol(zQ,zEnd);` |
|     14 |  553 | `					(*pnLine)++;` |
|     14 |  554 | `					continue;` |
|      - |  555 | `				}` |
|     58 |  556 | `				zQ++;` |
|    ! 0 |  557 | `			}` |
|     12 |  558 | `			if( zQ >= zEnd ){` |
|      - |  559 | `				/* the quote is what ran out, and php names what it wanted` |
|      - |  560 | ``				 * instead of the `]` it never reached */`` |
|      4 |  561 | `				*piBad = PH7_INI_STOP_SECTION_STR;` |
|      4 |  562 | `				return 0;` |
|      - |  563 | `			}` |
|      8 |  564 | `			z = &zQ[1];` |
|      8 |  565 | `			continue;` |
|      - |  566 | `		}` |
|    356 |  567 | `		if( c == '\'' ){` |
|     10 |  568 | `			const char *zQ = &z[1];` |
|     46 |  569 | `			while( zQ < zEnd && zQ[0] != '\'' ){` |
|     36 |  570 | `				zQ++;` |
|    ! 0 |  571 | `			}` |
|      - |  572 | `			/* php's raw rule wants at least one byte inside the quotes, so an` |
|      - |  573 | ``			 * empty `''` matches nothing at all and the run dies on the quote`` |
|      - |  574 | ``			 * itself -- still `expecting ']'`, on the line the counter is on. */`` |
|     10 |  575 | `			if( zQ >= zEnd \|\| zQ == &z[1] ){` |
|      4 |  576 | `				return 0;` |
|      - |  577 | `			}` |
|      6 |  578 | `			z = &zQ[1];` |
|      6 |  579 | `			continue;` |
|      - |  580 | `		}` |
|    346 |  581 | `		if( c == '$' && &z[1] < zEnd && z[1] == '{' ){` |
|     52 |  582 | `			unsigned int nAt = *pnLine;` |
|     52 |  583 | `			const char *zAfter = PHL_IniVarRun(z,zEnd,pnLine);` |
|     52 |  584 | `			PHL_ScreenIniVar(pEngine,z,(size_t)(zEnd - z),zFile,nAt);` |
|     52 |  585 | `			if( zAfter == 0 ){` |
|      - |  586 | `				/* Nothing closed it, so the screen just queued is a refusal` |
|      - |  587 | ``				 * whatever else the name holds: the `]` this run will not`` |
|      - |  588 | `				 * reach is never reported, because the entry above takes the` |
|      - |  589 | `				 * source down before that one is applied. */` |
|     20 |  590 | `				return 0;` |
|      - |  591 | `			}` |
|     32 |  592 | `			z = zAfter;` |
|     32 |  593 | `			continue;` |
|      - |  594 | `		}` |
|    294 |  595 | `		if( c == '\\' && &z[1] < zEnd ){` |
|      4 |  596 | `			z += 2;   /* whatever it is, a newline included, uncounted */` |
|      4 |  597 | `			continue;` |
|      - |  598 | `		}` |
|    290 |  599 | `		z++;` |
|    ! 0 |  600 | `	}` |
|      2 |  601 | `	return 0;` |
|     85 |  602 | `}` |
|      - |  603 | `/*` |
|      - |  604 | `` * Hand over an offset statement whose `]` was not followed by an `=`. zStmt is`` |
|      - |  605 | `` * what stands where php's parser wants that `=`, to the end of its line; the`` |
|      - |  606 | ` * engine owns the table that names the token in it.` |
|      - |  607 | ` */` |
|     24 |  608 | `static void PHL_RefuseIniOffset(ph7 *pEngine,const char *zStmt,size_t nStmt,` |
|      - |  609 | `	const char *zFile,unsigned int nLine,int bEof)` |
|    ! 0 |  610 | `{` |
|      - |  611 | `	char zStack[512];` |
|     24 |  612 | `	char *zText = zStack;` |
|     24 |  613 | `	if( nStmt + 1 > sizeof(zStack) ){` |
|    ! 0 |  614 | `		nStmt = sizeof(zStack) - 1;   /* the token is at the front of it */` |
|    ! 0 |  615 | `	}` |
|     24 |  616 | `	if( nStmt > 0 ){` |
|     18 |  617 | `		memcpy(zText,zStmt,nStmt);` |
|      9 |  618 | `	}` |
|     24 |  619 | `	zText[nStmt] = 0;` |
|     36 |  620 | `	ph7_config(pEngine,PH7_CONFIG_INI_ENTRY,zText,"",zFile,nLine,` |
|     12 |  621 | `		bEof ? PH7_INI_STOP_OFFSET_EOF : PH7_INI_STOP_OFFSET);` |
|     24 |  622 | `}` |
|      - |  623 | `/*` |
|      - |  624 | ` * Walk one whole php.ini source -- a -c file's bytes, or the buffer php's CLI` |
|      - |  625 | ` * builds out of every -d -- and apply the directives it holds. nLine is the line` |
|      - |  626 | ` * the first byte sits on: 1 for a file, and 6 for the -d buffer (see the caller).` |
|      - |  627 | ` */` |
|    805 |  628 | `static void PHL_ScanIniSource(ph7 *pEngine,const char *zSrc,size_t nSrc,` |
|      - |  629 | `	const char *zFile,unsigned int nLine)` |
|      4 |  630 | `{` |
|    809 |  631 | `	const char *z = zSrc;` |
|    809 |  632 | `	const char *zEnd = &zSrc[nSrc];` |
|   3671 |  633 | `	while( z < zEnd ){` |
|      - |  634 | `		const char *zName,*zNameEnd,*zVal,*zValEnd;` |
|      - |  635 | `		unsigned int nDir;` |
|   2952 |  636 | `		int bRunaway = 0;` |
|      - |  637 | `		int iStop;` |
|   2952 |  638 | `		int bTab = 0;` |
|   2952 |  639 | `		int bOffset = 0;` |
|   2952 |  640 | `		const char *zBlank = z;` |
|   4476 |  641 | `		while( z < zEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|     34 |  642 | `			bTab = bTab \|\| z[0] == '\t';` |
|     34 |  643 | `			z++;` |
|    ! 0 |  644 | `		}` |
|   2952 |  645 | `		if( z >= zEnd ){` |
|    ! 0 |  646 | `			break;` |
|      - |  647 | `		}` |
|   2952 |  648 | `		if( z[0] == '\n' \|\| z[0] == '\r' ){` |
|   1379 |  649 | `			z = PHL_IniEatEol(z,zEnd);` |
|   1379 |  650 | `			nLine++;` |
|   1379 |  651 | `			continue;` |
|      - |  652 | `		}` |
|   1577 |  653 | `		if( z[0] == ';' \|\| z[0] == '#' ){` |
|      - |  654 | `			/* comments: the newline behind them is counted above */` |
|      8 |  655 | `			z = PHL_IniSkipEol(z,zEnd);` |
|      8 |  656 | `			continue;` |
|      - |  657 | `		}` |
|      - |  658 | `		/* Which rule ate the blank run decides what statement this is. php's` |
|      - |  659 | ``		 * `{TABS_AND_SPACES}*[=]` and its comment and newline rules outrun`` |
|      - |  660 | `		 * everything, so a run in front of one of those is gone whatever it` |
|      - |  661 | `		 * held. Failing them, only a run holding a TAB reaches the rule that` |
|      - |  662 | ``		 * throws a run away: a SPACES-only run is inside the `{LABEL}` or the`` |
|      - |  663 | ``		 * `{LABEL}"["` behind it, which then trims it back off the name. So`` |
|      - |  664 | ``		 * `\t[s]` is a section header and `  [s]` is the OFFSET `  `["s"] --`` |
|      - |  665 | ``		 * an entry when an `=` follows it, and a syntax error when none does. */`` |
|   1569 |  666 | `		if( z[0] != '=' && !bTab ){` |
|   1555 |  667 | `			z = zBlank;` |
|    775 |  668 | `		}` |
|   1569 |  669 | `		if( z[0] == '[' ){` |
|      - |  670 | `			/* A section NAME is one scanner run, and the only thing that closes` |
|      - |  671 | `` 			 * it is the `]`. A newline does not: php's rule has none, so a `[` `` |
|      - |  672 | ``			 * with no `]` behind it on its own line runs out of source and`` |
|      - |  673 | ``			 * refuses the file -- `unexpected end of file, expecting ']'`,`` |
|      - |  674 | `			 * dated where the run stopped, with every directive ABOVE it still` |
|      - |  675 | `			 * standing and every one below it inside a header that never closed.` |
|      - |  676 | ``			 * A `]` written further down does not rescue it. Inside the name a`` |
|      - |  677 | ``			 * double-quoted run hides a `]` AND crosses newlines (counting`` |
|      - |  678 | ``			 * them, as it does in a value); a raw `'` run and a `${}` hide one`` |
|      - |  679 | `			 * without counting; and a backslash carries the byte behind it,` |
|      - |  680 | `			 * the newline included. */` |
|      - |  681 | `			unsigned int nSec;` |
|      - |  682 | `			int iBad;` |
|      - |  683 | `			const char *zAfter;` |
|    118 |  684 | `			z++;` |
|    118 |  685 | `			nSec = nLine;` |
|    118 |  686 | `			zAfter = PHL_IniBracketRun(pEngine,zFile,z,zEnd,&nSec,&iBad);` |
|    118 |  687 | `			if( zAfter == 0 ){` |
|     52 |  688 | `				PHL_RefuseIniSource(pEngine,zFile,nSec,iBad);` |
|     52 |  689 | `				return;` |
|      - |  690 | `			}` |
|     66 |  691 | `			nLine = nSec;` |
|     66 |  692 | `			z = zAfter;` |
|      - |  693 | ``			/* php's rule for the `]` that closes a header eats the blanks`` |
|      - |  694 | `			 * behind it, eats a newline when one is there, and counts a line` |
|      - |  695 | `			 * either way. So a directive written on the header's own line is` |
|      - |  696 | ``			 * read like any other -- `[s] precision=9` sets precision -- and`` |
|      - |  697 | `			 * a refusal below a header that did not end its own line is dated` |
|      - |  698 | `			 * one line lower than it was typed. */` |
|    105 |  699 | `			while( z < zEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|      6 |  700 | `				z++;` |
|    ! 0 |  701 | `			}` |
|     66 |  702 | `			if( z < zEnd && (z[0] == '\n' \|\| z[0] == '\r') ){` |
|     56 |  703 | `				z = PHL_IniEatEol(z,zEnd);` |
|     28 |  704 | `			}` |
|     66 |  705 | `			nLine++;` |
|     66 |  706 | `			continue;` |
|      - |  707 | `		}` |
|   1451 |  708 | `		nDir = nLine;` |
|   1451 |  709 | `		zName = z;` |
|      - |  710 | ``		/* php's `{LABEL}` is a run of LABEL_CHARs, and a TAB is not one: it`` |
|      - |  711 | ``		 * ends the name and the statement with it, so `xx\tprecision=9` is a`` |
|      - |  712 | `		 * bare label php drops and then an ordinary entry. The bytes its` |
|      - |  713 | `		 * operators and brackets are made of are not LABEL_CHARs either, but` |
|      - |  714 | `		 * they travel INSIDE the text handed over below -- the engine owns the` |
|      - |  715 | `		 * table that says which of them refuse the source -- and only the` |
|      - |  716 | `		 * three that open a statement of their own stop the run out here. */` |
|  17371 |  717 | `		while( z < zEnd && z[0] != '=' && z[0] != '\n' && z[0] != '\r'` |
|  22514 |  718 | `		 && z[0] != '\t' && z[0] != ';' && z[0] != '[' ){` |
|  14481 |  719 | `			z++;` |
|      4 |  720 | `		}` |
|   1451 |  721 | `		if( z < zEnd && z[0] == '\t' ){` |
|      - |  722 | ``			/* `{TABS_AND_SPACES}*[=]` outruns the rule that throws a blank run`` |
|      - |  723 | ``			 * away, so a TAB with nothing but blanks between it and an `=` is`` |
|      - |  724 | `			 * still this directive's; anything else behind it is a statement of` |
|      - |  725 | `			 * its own, and this one ends here carrying no value. */` |
|     10 |  726 | `			const char *zPeek = z;` |
|     25 |  727 | `			while( zPeek < zEnd && (zPeek[0] == ' ' \|\| zPeek[0] == '\t') ){` |
|     10 |  728 | `				zPeek++;` |
|    ! 0 |  729 | `			}` |
|     10 |  730 | `			if( zPeek < zEnd && zPeek[0] == '=' ){` |
|      4 |  731 | `				z = zPeek;` |
|      2 |  732 | `			}` |
|      5 |  733 | `		}` |
|   1451 |  734 | `		bOffset = z < zEnd && z[0] == '[';` |
|   1451 |  735 | `		zNameEnd = z;` |
|   2502 |  736 | `		while( zNameEnd > zName && (zNameEnd[-1] == ' ' \|\| zNameEnd[-1] == '\t') ){` |
|    328 |  737 | `			zNameEnd--;` |
|    ! 0 |  738 | `		}` |
|   1451 |  739 | `		if( bOffset ){` |
|      - |  740 | ``			/* `{LABEL}"["` is one token, so the `[` goes to the engine WITH the`` |
|      - |  741 | ``			 * run in front of it: that is how it tells `on[x] = 1`, an entry`` |
|      - |  742 | ``			 * whose bool word the offset rule outran, from `x&y[z] = 1`, whose`` |
|      - |  743 | ``			 * `&` ended the LABEL long before any offset could form. */`` |
|     52 |  744 | `			unsigned int nOff = nLine;` |
|      - |  745 | `			const char *zAfter;` |
|      - |  746 | `			int iBad;` |
|     52 |  747 | `			PHL_ScreenIniStmt(pEngine,zName,(size_t)(z + 1 - zName),zFile,nDir);` |
|     52 |  748 | `			zAfter = PHL_IniBracketRun(pEngine,zFile,&z[1],zEnd,&nOff,&iBad);` |
|     52 |  749 | `			if( zAfter == 0 ){` |
|     10 |  750 | `				PHL_RefuseIniSource(pEngine,zFile,nOff,iBad);` |
|     22 |  751 | `				return;` |
|      - |  752 | `			}` |
|     42 |  753 | `			nLine = nOff;` |
|     42 |  754 | `			z = zAfter;` |
|     42 |  755 | `			zNameEnd = z;` |
|      - |  756 | ``			/* `TC_OFFSET option_offset ']' '='` is the only statement php's`` |
|      - |  757 | ``			 * grammar builds out of an offset. Its `]` eats no newline and`` |
|      - |  758 | `			 * counts no line, so what follows is read at a statement position` |
|      - |  759 | ``			 * on this very line -- and anything but an `=` there refuses the`` |
|      - |  760 | `			 * source under the token that state makes of it. */` |
|      - |  761 | `			{` |
|     42 |  762 | `				const char *zPeek = z;` |
|     67 |  763 | `				while( zPeek < zEnd && (zPeek[0] == ' ' \|\| zPeek[0] == '\t') ){` |
|      4 |  764 | `					zPeek++;` |
|    ! 0 |  765 | `				}` |
|     42 |  766 | `				if( zPeek >= zEnd \|\| zPeek[0] != '=' ){` |
|     24 |  767 | `					const char *zLine = PHL_IniSkipEol(z,zEnd);` |
|     36 |  768 | `					PHL_RefuseIniOffset(pEngine,z,(size_t)(zLine - z),` |
|     12 |  769 | `						zFile,nLine,zLine >= zEnd);` |
|     24 |  770 | `					return;` |
|      - |  771 | `				}` |
|     18 |  772 | `				z = zPeek;` |
|      - |  773 | `			}` |
|      9 |  774 | `		}` |
|      - |  775 | ``		/* An `=` with nothing in front of it leaves the run empty, and php's`` |
|      - |  776 | `		 * scanner has a token there all the same. An offset was screened above,` |
|      - |  777 | ``		 * with the `[` that decides how its run is read. */`` |
|   1417 |  778 | `		if( !bOffset ){` |
|   2794 |  779 | `			PHL_ScreenIniStmt(pEngine,zName,` |
|   1395 |  780 | `				zNameEnd > zName ? (size_t)(zNameEnd - zName) : (size_t)(z < zEnd ? 1 : 0),` |
|    697 |  781 | `				zFile,nDir);` |
|    697 |  782 | `		}` |
|   1417 |  783 | `		if( z >= zEnd \|\| z[0] != '=' ){` |
|      - |  784 | `			/* php's php.ini callback ignores a statement that carries no` |
|      - |  785 | `			 * value, so a bare name neither defines the entry nor sets it to` |
|      - |  786 | ``			 * "1" -- `parse_ini_string("precision")` is the empty array on`` |
|      - |  787 | `			 * both sides of the fence for the same reason. */` |
|     44 |  788 | `			continue;` |
|      - |  789 | `		}` |
|   1373 |  790 | `		z++;   /* past the '=' */` |
|   1373 |  791 | `		zVal = z;` |
|   6924 |  792 | `		while( z < zEnd ){` |
|   6912 |  793 | `			int c = (unsigned char)z[0];` |
|   6912 |  794 | `			if( c == '\n' \|\| c == '\r' \|\| c == ';' ){` |
|    673 |  795 | `				break;` |
|      - |  796 | `			}` |
|   5573 |  797 | `			if( c == '"' ){` |
|     69 |  798 | `				const char *zQ = &z[1];` |
|     69 |  799 | `				unsigned int nEat = 0;` |
|    509 |  800 | `				while( zQ < zEnd && zQ[0] != '"' ){` |
|    443 |  801 | `					if( zQ[0] == '\\' && &zQ[1] < zEnd ){` |
|      - |  802 | ``						/* a tool that has to put a quote in a value writes `\"` */`` |
|    ! 0 |  803 | `						zQ += 2;` |
|    ! 0 |  804 | `						continue;` |
|      - |  805 | `					}` |
|    443 |  806 | `					if( zQ[0] == '\n' \|\| zQ[0] == '\r' ){` |
|     32 |  807 | `						zQ = PHL_IniEatEol(zQ,zEnd);` |
|     32 |  808 | `						nEat++;` |
|     32 |  809 | `						continue;` |
|      - |  810 | `					}` |
|    411 |  811 | `					zQ++;` |
|      3 |  812 | `				}` |
|     69 |  813 | `				nLine += nEat;` |
|     69 |  814 | `				if( zQ >= zEnd ){` |
|     10 |  815 | `					bRunaway = 1;` |
|     10 |  816 | `					z = zEnd;` |
|     10 |  817 | `					break;` |
|      - |  818 | `				}` |
|     59 |  819 | `				z = &zQ[1];` |
|     59 |  820 | `				continue;` |
|      - |  821 | `			}` |
|   5507 |  822 | `			if( c == '\'' ){` |
|     22 |  823 | `				const char *zQ = &z[1];` |
|    242 |  824 | `				while( zQ < zEnd && zQ[0] != '\'' ){` |
|    220 |  825 | `					zQ++;` |
|    ! 0 |  826 | `				}` |
|     22 |  827 | `				if( zQ >= zEnd ){` |
|      6 |  828 | `					bRunaway = 1;` |
|      6 |  829 | `					z = zEnd;` |
|      6 |  830 | `					break;` |
|      - |  831 | `				}` |
|     16 |  832 | `				z = &zQ[1];   /* no line counting: the raw rule is one match */` |
|     16 |  833 | `				continue;` |
|      - |  834 | `			}` |
|   5485 |  835 | `			if( c == '$' ){` |
|      - |  836 | ``				/* php's VALUE_CHARS unit `("$"[^{])` carries the byte behind`` |
|      - |  837 | ``				 * the `$` whatever that byte is -- the NEWLINE included -- and`` |
|      - |  838 | `				 * one more when it is a backslash. So a value whose last byte` |
|      - |  839 | ``				 * is `$` does not end at its own line: it eats the newline and`` |
|      - |  840 | ``				 * runs on into the text below, which is how `x=a$` reaches the`` |
|      - |  841 | ``				 * next directive's `=` and refuses the file there. The line`` |
|      - |  842 | `				 * counter does NOT move for it: only php's own NEWLINE rule` |
|      - |  843 | `				 * counts, and this newline went to the value scanner instead,` |
|      - |  844 | `				 * so a refusal below a run-on is dated one line short of where` |
|      - |  845 | ``				 * it was typed. A `$` with nothing behind it matches nothing at`` |
|      - |  846 | `				 * all, and the value stops in front of it. */` |
|    126 |  847 | `				if( &z[1] >= zEnd ){` |
|      2 |  848 | `					break;` |
|      - |  849 | `				}` |
|    124 |  850 | `				if( z[1] == '{' ){` |
|     94 |  851 | ``					z++;   /* `${NAME}` is its own production, not this unit */`` |
|     94 |  852 | `					continue;` |
|      - |  853 | `				}` |
|     30 |  854 | `				z += z[1] == '\\' && &z[2] < zEnd ? 3 : 2;` |
|     30 |  855 | `				continue;` |
|      - |  856 | `			}` |
|   5359 |  857 | `			z++;` |
|      4 |  858 | `		}` |
|   1373 |  859 | `		zValEnd = z;` |
|   1373 |  860 | `		iStop = PH7_INI_STOP_EOF;` |
|   1373 |  861 | `		if( !bRunaway ){` |
|      - |  862 | ``			/* the `;` comment, and the newline that closes the directive, are`` |
|      - |  863 | `			 * the scanner's own tokens rather than part of the value */` |
|   1357 |  864 | `			int bComment = z < zEnd && z[0] == ';';` |
|   1357 |  865 | `			z = PHL_IniSkipEol(z,zEnd);` |
|      - |  866 | `			/* Which of the two php refuses a value that merely ran out under.` |
|      - |  867 | `			 * Its NEWLINE rule counts the line it eats, so a directive closed by` |
|      - |  868 | `			 * one is dated a line low; a source that ends without one has no` |
|      - |  869 | `			 * newline to give, and a comment left hanging there matches nothing` |
|      - |  870 | `			 * at all -- what the parser gets then is the end of the input. */` |
|   1357 |  871 | `			iStop = z < zEnd ? PH7_INI_STOP_EOL` |
|    688 |  872 | `				: bComment ? PH7_INI_STOP_COMMENT : PH7_INI_STOP_EOF;` |
|    676 |  873 | `		}` |
|   2057 |  874 | `		PHL_ApplyIniValue(pEngine,zName,(size_t)(zNameEnd - zName),` |
|   1369 |  875 | `			zVal,(size_t)(zValEnd - zVal),zFile,nDir,iStop);` |
|      4 |  876 | `	}` |
|    406 |  877 | `}` |
|      - |  878 | `/*` |
|      - |  879 | `` * One -d, as php's ini builder writes it into that buffer: `name=value` and a`` |
|      - |  880 | ` * newline, with the value wrapped in double quotes whenever its first byte is` |
|      - |  881 | ` * not alphanumeric and not already a quote -- php's own workaround, applied to` |
|      - |  882 | ``  * the argv text before any trimming, and what makes `-d 'x=(E_ALL) ^ E_NOTICE'` `` |
|      - |  883 | `` * and `-d 'x=~~2'` store their own text where the same lines in a -c file are`` |
|      - |  884 | `` * expressions. A bare `-d name` is written `name=1`. Returns the bytes written.`` |
|      - |  885 | ` */` |
|    505 |  886 | `static size_t PHL_IniDefineLine(const char *zPair,char *zOut)` |
|      4 |  887 | `{` |
|    509 |  888 | `	const char *zEq = strchr(zPair,'=');` |
|      - |  889 | `	size_t n;` |
|    509 |  890 | `	if( zEq == 0 ){` |
|      2 |  891 | `		n = strlen(zPair);` |
|      2 |  892 | `		memcpy(zOut,zPair,n);` |
|      2 |  893 | `		memcpy(&zOut[n],"=1\n",sizeof("=1\n")-1);` |
|      2 |  894 | `		return n + sizeof("=1\n") - 1;` |
|      - |  895 | `	}` |
|      - |  896 | `	{` |
|    507 |  897 | `		const char *zV = &zEq[1];` |
|    507 |  898 | `		size_t nHead = (size_t)(zEq - zPair) + 1;   /* the name and its '=' */` |
|    507 |  899 | `		size_t nV = strlen(zV);` |
|    751 |  900 | `		int bQuote = zV[0] != 0 && zV[0] != '"' && zV[0] != '\''` |
|    911 |  901 | `		          && !((zV[0] >= '0' && zV[0] <= '9')` |
|    324 |  902 | `		            \|\| (zV[0] >= 'a' && zV[0] <= 'z')` |
|    123 |  903 | `		            \|\| (zV[0] >= 'A' && zV[0] <= 'Z'));` |
|    507 |  904 | `		memcpy(zOut,zPair,nHead);` |
|    507 |  905 | `		n = nHead;` |
|    507 |  906 | `		if( bQuote ){` |
|     35 |  907 | `			zOut[n++] = '"';` |
|     16 |  908 | `		}` |
|    507 |  909 | `		if( nV > 0 ){` |
|    503 |  910 | `			memcpy(&zOut[n],zV,nV);` |
|    503 |  911 | `			n += nV;` |
|    249 |  912 | `		}` |
|    507 |  913 | `		if( bQuote ){` |
|     35 |  914 | `			zOut[n++] = '"';` |
|     16 |  915 | `		}` |
|    507 |  916 | `		zOut[n++] = '\n';` |
|    507 |  917 | `		return n;` |
|      - |  918 | `	}` |
|    256 |  919 | `}` |
|      - |  920 | `/*` |
|      - |  921 | ` * Expand one path to the absolute, canonical name php would quote for it.` |
|      - |  922 | `` * php runs the php.ini it opened through expand_filepath(), so `.`, `..` and a`` |
|      - |  923 | ` * symlink are all resolved before the name reaches a diagnostic or` |
|      - |  924 | ` * php_ini_loaded_file(). Writes a heap string to *pzOut (caller frees) and` |
|      - |  925 | ` * returns TRUE; on failure leaves *pzOut alone and returns FALSE, and the raw` |
|      - |  926 | ` * argument is used instead -- the file is already open by then, so there is` |
|      - |  927 | ` * nothing to report and php has no answer for this case either.` |
|      - |  928 | ` */` |
|    514 |  929 | `static int PHL_ExpandPath(const char *zPath,char **pzOut)` |
|    ! 0 |  930 | `{` |
|    514 |  931 | `	char *zReal = 0;` |
|      - |  932 | `#ifdef __UNIXES__` |
|    514 |  933 | `	zReal = realpath(zPath,0);    /* POSIX: malloc'd result */` |
|      - |  934 | `#endif` |
|      - |  935 | `#ifdef __WINNT__` |
|    ! 0 |  936 | `	zReal = _fullpath(0,zPath,0); /* MSVC CRT: malloc'd absolute path */` |
|      - |  937 | `#endif` |
|    514 |  938 | `	if( zReal ){` |
|    514 |  939 | `		*pzOut = zReal;` |
|    514 |  940 | `		return 1;` |
|      - |  941 | `	}` |
|    ! 0 |  942 | `	(void)zPath;` |
|    ! 0 |  943 | `	return 0;` |
|    257 |  944 | `}` |
|      - |  945 | `/*` |
|      - |  946 | ` * Open one php.ini candidate, refusing a directory the way php's loader does:` |
|      - |  947 | ` * it stats the name first and only ever fopen()s a non-directory. Without that,` |
|      - |  948 | `` * a `-c <dir>` would load no directives in silence -- an opened directory reads`` |
|      - |  949 | ` * zero bytes -- where php loads the php.ini inside it.` |
|      - |  950 | ` * Returns the handle and writes the canonical name to *pzName.` |
|      - |  951 | ` */` |
|    540 |  952 | `static FILE * PHL_OpenIniCandidate(const char *zPath,char **pzName)` |
|    ! 0 |  953 | `{` |
|      - |  954 | `	FILE *pFile;` |
|      - |  955 | `#ifdef __UNIXES__` |
|      - |  956 | `	/* Only POSIX needs asking: fopen() there OPENS a directory and then reads` |
|      - |  957 | `	 * nothing. The Windows CRT refuses one outright, which is the same answer. */` |
|      - |  958 | `	struct stat sBuf;` |
|    540 |  959 | `	if( stat(zPath,&sBuf) != 0 \|\| (sBuf.st_mode & S_IFMT) == S_IFDIR ){` |
|     26 |  960 | `		return 0;` |
|      - |  961 | `	}` |
|      - |  962 | `#endif` |
|      - |  963 | `	/* MSVC deprecates fopen() in favour of fopen_s() under /W4 /WX. The CRT call` |
|      - |  964 | `	 * is what every other host here uses and its failure is handled right below,` |
|      - |  965 | `	 * so silence the one call rather than the file (builtin_date.c's precedent). */` |
|      - |  966 | `#if defined(_MSC_VER) && _MSC_VER >= 1400` |
|      - |  967 | `#pragma warning(push)` |
|      - |  968 | `#pragma warning(disable:4996)` |
|      - |  969 | `#endif` |
|    514 |  970 | `	pFile = fopen(zPath,"rb");` |
|      - |  971 | `#if defined(_MSC_VER) && _MSC_VER >= 1400` |
|      - |  972 | `#pragma warning(pop)` |
|      - |  973 | `#endif` |
|    514 |  974 | `	if( pFile == 0 ){` |
|    ! 0 |  975 | `		return 0;` |
|      - |  976 | `	}` |
|    514 |  977 | `	if( !PHL_ExpandPath(zPath,pzName) ){` |
|    ! 0 |  978 | `		*pzName = 0;` |
|    ! 0 |  979 | `	}` |
|    514 |  980 | `	return pFile;` |
|    270 |  981 | `}` |
|      - |  982 | `/*` |
|      - |  983 | ` * Find the php.ini a -c argument names, exactly where php looks for it. The` |
|      - |  984 | ` * argument is first tried as a FILE; anything else -- a directory, a name that` |
|      - |  985 | ` * is not there -- makes php fall through to its ini SEARCH PATH, which under` |
|      - |  986 | `` * `-c` is the argument itself, and there it asks for `php-<sapi>.ini` and then`` |
|      - |  987 | `` * `php.ini`. So `-c /etc/php` reads `/etc/php/php-cli.ini` if there is one.`` |
|      - |  988 | ` * Nothing found is not an error: php starts on its built-in defaults and says` |
|      - |  989 | ` * nothing, on either stream.` |
|      - |  990 | ` */` |
|    520 |  991 | `static FILE * PHL_FindIniFile(const char *zPath,char **pzName)` |
|    ! 0 |  992 | `{` |
|      - |  993 | `	static const char * const azInDir[] = { "php-cli.ini", "php.ini" };` |
|    520 |  994 | `	size_t nPath = strlen(zPath);` |
|      - |  995 | `	FILE *pFile;` |
|      - |  996 | `	unsigned int i;` |
|    520 |  997 | `	if( nPath < 1 ){` |
|    ! 0 |  998 | `		return 0;` |
|      - |  999 | `	}` |
|    520 | 1000 | `	pFile = PHL_OpenIniCandidate(zPath,pzName);` |
|    520 | 1001 | `	if( pFile ){` |
|    508 | 1002 | `		return pFile;` |
|      - | 1003 | `	}` |
|     26 | 1004 | `	for( i = 0 ; i < sizeof(azInDir)/sizeof(azInDir[0]) ; ++i ){` |
|     20 | 1005 | `		size_t nTry = nPath + 1 + strlen(azInDir[i]) + 1;` |
|     20 | 1006 | `		char *zTry = (char *)malloc(nTry);` |
|     20 | 1007 | `		if( zTry == 0 ){` |
|    ! 0 | 1008 | `			return 0;` |
|      - | 1009 | `		}` |
|     20 | 1010 | `		snprintf(zTry,nTry,"%s/%s",zPath,azInDir[i]);` |
|     20 | 1011 | `		pFile = PHL_OpenIniCandidate(zTry,pzName);` |
|     20 | 1012 | `		free(zTry);` |
|     20 | 1013 | `		if( pFile ){` |
|      6 | 1014 | `			return pFile;` |
|      - | 1015 | `		}` |
|      7 | 1016 | `	}` |
|      6 | 1017 | `	return 0;` |
|    260 | 1018 | `}` |
|      - | 1019 | `/*` |
|      - | 1020 | ` * Load php.ini directives from a -c argument. Read whole: a value may span lines` |
|      - | 1021 | ` * (see PHL_ScanIniSource), so no line of it can be read on its own.` |
|      - | 1022 | ` */` |
|    520 | 1023 | `static void PHL_LoadIniFile(ph7 *pEngine,const char *zPath)` |
|    ! 0 | 1024 | `{` |
|      - | 1025 | `	char *zSrc;` |
|    520 | 1026 | `	char *zName = 0;` |
|      - | 1027 | `	long nSize;` |
|      - | 1028 | `	size_t nRead;` |
|    520 | 1029 | `	FILE *pFile = PHL_FindIniFile(zPath,&zName);` |
|    520 | 1030 | `	if( pFile == 0 ){` |
|      - | 1031 | `		/* php reads no file and starts on its defaults, in silence. */` |
|      6 | 1032 | `		return;` |
|      - | 1033 | `	}` |
|    514 | 1034 | `	if( fseek(pFile,0,SEEK_END) != 0 \|\| (nSize = ftell(pFile)) < 0` |
|    514 | 1035 | `	 \|\| fseek(pFile,0,SEEK_SET) != 0 ){` |
|    ! 0 | 1036 | `		fclose(pFile);` |
|    ! 0 | 1037 | `		free(zName);` |
|    ! 0 | 1038 | `		return;` |
|      - | 1039 | `	}` |
|    514 | 1040 | `	zSrc = (char *)malloc((size_t)nSize + 1);` |
|    514 | 1041 | `	if( zSrc == 0 ){` |
|    ! 0 | 1042 | `		fclose(pFile);` |
|    ! 0 | 1043 | `		free(zName);` |
|    ! 0 | 1044 | `		return;` |
|      - | 1045 | `	}` |
|    514 | 1046 | `	nRead = fread(zSrc,1,(size_t)nSize,pFile);` |
|    514 | 1047 | `	fclose(pFile);` |
|    514 | 1048 | `	zSrc[nRead] = 0;` |
|      - | 1049 | `	/* Every reader of this file's name -- a refusal the ini grammar dates, and` |
|      - | 1050 | `	 * php_ini_loaded_file() -- gets the same canonical string. */` |
|    514 | 1051 | `	ph7_config(pEngine,PH7_CONFIG_INI_FILE,zName ? zName : zPath);` |
|    514 | 1052 | `	PHL_ScanIniSource(pEngine,zSrc,nRead,zName ? zName : zPath,1);` |
|    514 | 1053 | `	free(zSrc);` |
|    514 | 1054 | `	free(zName);` |
|    260 | 1055 | `}` |
|      - | 1056 | `/*` |
|      - | 1057 | ` * Return TRUE when standard input is a pipe/redirect rather than an interactive` |
|      - | 1058 | ` * terminal. php reads the script from stdin in exactly that case; a terminal` |
|      - | 1059 | ` * would block waiting for input, so there we keep the usage error instead.` |
|      - | 1060 | ` */` |
|     10 | 1061 | `static int PHL_StdinIsPipe(void)` |
|    ! 0 | 1062 | `{` |
|      - | 1063 | `#ifdef __UNIXES__` |
|     10 | 1064 | `	return !isatty(0);` |
|      - | 1065 | `#else` |
|    ! 0 | 1066 | `	return 0;` |
|      - | 1067 | `#endif` |
|    ! 0 | 1068 | `}` |
|      - | 1069 | `/*` |
|      - | 1070 | ` * Slurp all of standard input into a heap buffer (NUL-terminated). Returns the` |
|      - | 1071 | ` * buffer (caller owns it, though the process exits shortly after) and writes the` |
|      - | 1072 | ` * byte length to *pnLen, or NULL on allocation failure.` |
|      - | 1073 | ` */` |
|     12 | 1074 | `static char * PHL_SlurpStdin(int *pnLen)` |
|    ! 0 | 1075 | `{` |
|     12 | 1076 | `	size_t nCap = 8192, nUsed = 0;` |
|     12 | 1077 | `	char *zBuf = (char *)malloc(nCap);` |
|     12 | 1078 | `	if( zBuf == 0 ){ return 0; }` |
|      6 | 1079 | `	for(;;){` |
|      - | 1080 | `		size_t nRd;` |
|     12 | 1081 | `		if( nUsed + 4096 + 1 > nCap ){` |
|      - | 1082 | `			char *zNew;` |
|    ! 0 | 1083 | `			nCap *= 2;` |
|    ! 0 | 1084 | `			zNew = (char *)realloc(zBuf, nCap);` |
|    ! 0 | 1085 | `			if( zNew == 0 ){ free(zBuf); return 0; }` |
|    ! 0 | 1086 | `			zBuf = zNew;` |
|    ! 0 | 1087 | `		}` |
|     12 | 1088 | `		nRd = fread(zBuf + nUsed, 1, 4096, stdin);` |
|     12 | 1089 | `		nUsed += nRd;` |
|     12 | 1090 | `		if( nRd < 4096 ){ break; }` |
|    ! 0 | 1091 | `	}` |
|     12 | 1092 | `	zBuf[nUsed] = 0;` |
|     12 | 1093 | `	*pnLen = (int)nUsed;` |
|     12 | 1094 | `	return zBuf;` |
|      6 | 1095 | `}` |
|      - | 1096 | `/*` |
|      - | 1097 | ` * Main program: Compile and execute the PHP file.` |
|      - | 1098 | ` */` |
|   7857 | 1099 | `int main(int argc,char **argv)` |
|      5 | 1100 | `{` |
|      - | 1101 | `	ph7 *pEngine; /* PH7 engine */` |
|      - | 1102 | `	ph7_vm *pVm;  /* Compiled PHP program */` |
|   7862 | 1103 | `	int dump_vm = 0;    /* Dump VM instructions if TRUE */` |
|   7862 | 1104 | `	int run_code = 0;    /* Run inline code if TRUE */` |
|   7862 | 1105 | `	int lint_mode = 0;   /* Syntax-check only (-l) if TRUE */` |
|   7862 | 1106 | `	const char *zRunCode = 0; /* Inline code string */` |
|   7862 | 1107 | `	int stdin_code = 0;       /* Execute a script read from stdin if TRUE */` |
|   7862 | 1108 | `	char *zStdinCode = 0;     /* Script slurped from stdin */` |
|   7862 | 1109 | `	int nStdinCode = 0;       /* Length of the stdin script */` |
|   7862 | 1110 | ``	int dash_dash = 0;        /* Saw `--`: read from stdin, rest are script args */`` |
|      - | 1111 | `#ifdef PHL_ENABLE_SERVER` |
|   7862 | 1112 | `	int server_mode = 0;        /* Start built-in server if TRUE */` |
|   7862 | 1113 | `	const char *zServerAddr = 0; /* host:port string */` |
|   7862 | 1114 | `	const char *zDocRoot = ".";  /* Document root */` |
|      - | 1115 | `#endif` |
|      - | 1116 | `	int n;              /* Script arguments */` |
|      - | 1117 | `	int rc;` |
|      - | 1118 | `	const char *azIniDefine[64]; /* -d name=value directives, in order */` |
|   7862 | 1119 | `	int nIniDefine = 0;` |
|   7862 | 1120 | `	const char *zIniFile = 0;    /* -c php.ini path */` |
|      - | 1121 | `	/* php's CLI ignores SIGPIPE for the whole process, and this is PHL's CLI: a` |
|      - | 1122 | `	 * write to a pipe or socket whose reader is gone then answers EPIPE instead of` |
|      - | 1123 | `	 * KILLING the interpreter, which is the answer every php program is written` |
|      - | 1124 | `	 * against. The disposition is inherited across fork AND exec, so it is also` |
|      - | 1125 | ``	 * what a proc_open() child runs with -- `cat` writing into a pipe the script`` |
|      - | 1126 | `	 * has closed exits 1 with a write error under php, where a default SIGPIPE` |
|      - | 1127 | `	 * kills it and proc_close() reports 141. The networking subsystem used to set` |
|      - | 1128 | `	 * this lazily at the first socket, so a program that opened none never had it. */` |
|      - | 1129 | `#if defined(SIGPIPE) && defined(SIG_IGN)` |
|   7857 | 1130 | `	signal(SIGPIPE,SIG_IGN);` |
|      - | 1131 | `#endif` |
|      - | 1132 | `	/* php's own module startup pins LC_CTYPE to C.UTF-8 and leaves every other` |
|      - | 1133 | `` 	 * category at C, whatever the environment says -- so `setlocale(LC_CTYPE,'0')` `` |
|      - | 1134 | `	 * answers "C.UTF-8" under php on a box with the locale and "C" on one without,` |
|      - | 1135 | `	 * and never the LANG the shell exported. That is observable on its own, and it` |
|      - | 1136 | `	 * also decides the encoding ext/gettext hands an answer back in when nothing` |
|      - | 1137 | `	 * called bind_textdomain_codeset(). The engine LIBRARY does not touch the` |
|      - | 1138 | `	 * process locale; this is the CLI, which is PHL's SAPI. */` |
|   7862 | 1139 | `	if( setlocale(LC_CTYPE,"C.UTF-8") == 0 ){` |
|      5 | 1140 | `		setlocale(LC_CTYPE,"C");` |
|    ! 0 | 1141 | `	}` |
|      - | 1142 | `	/* Process interpreter arguments first*/` |
|   9894 | 1143 | `	for(n = 1 ; n < argc ; ++n ){` |
|      - | 1144 | `		int c;` |
|   9355 | 1145 | `		if( argv[n][0] != '-' ){` |
|      - | 1146 | `			/* No more interpreter arguments */` |
|   7316 | 1147 | `			break;` |
|      - | 1148 | `		}` |
|      - | 1149 | `		/* Check for long options */` |
|   2043 | 1150 | `		if( argv[n][1] == '-' ){` |
|     25 | 1151 | `			if( argv[n][2] == 0 ){` |
|      - | 1152 | ``				/* php CLI parity: a bare `--` ends interpreter options; the`` |
|      - | 1153 | ``				 * script is read from stdin and everything after `--` becomes`` |
|      - | 1154 | `				 * the script's own arguments ($argv[1..]). */` |
|      2 | 1155 | `				dash_dash = 1;` |
|      2 | 1156 | `				n++;` |
|      2 | 1157 | `				break;` |
|      - | 1158 | `			}` |
|     23 | 1159 | `			if( strcmp(argv[n], "--version") == 0 ){` |
|      7 | 1160 | `				Version();` |
|     20 | 1161 | `			}else if( strcmp(argv[n], "--help") == 0 ){` |
|      3 | 1162 | `				Help();` |
|     15 | 1163 | `			}else if( strcmp(argv[n], "--rf") == 0 \|\| strcmp(argv[n], "--rc") == 0 ){` |
|      - | 1164 | ``				/* php CLI parity: `--rf <function>` / `--rc <class>` print the`` |
|      - | 1165 | `				 * Reflection export (the __toString machinery is byte-exact vs` |
|      - | 1166 | ``				 * php) and exit 1 with `Exception: <message>` when the target`` |
|      - | 1167 | `				 * does not exist. Implemented as an inline snippet riding the` |
|      - | 1168 | `				 * -r code path; the NAME is charset-validated (identifier +` |
|      - | 1169 | `				 * namespace separators) so it embeds safely in the snippet. */` |
|      - | 1170 | `				static char zReflCode[768];` |
|      7 | 1171 | `				const char *zWhat = (argv[n][3] == 'f') ? "ReflectionFunction" : "ReflectionClass";` |
|      - | 1172 | `				const char *zName;` |
|      - | 1173 | `				const char *zChk;` |
|      7 | 1174 | `				if( n + 1 >= argc ){` |
|    ! 0 | 1175 | `					FatalCode("Missing name argument for --rf/--rc",1);` |
|    ! 0 | 1176 | `				}` |
|      7 | 1177 | `				zName = argv[++n];` |
|     71 | 1178 | `				for( zChk = zName ; *zChk ; zChk++ ){` |
|     65 | 1179 | `					char ch = *zChk;` |
|     65 | 1180 | `					if( !(ch == '_' \|\| ch == '\\'` |
|     58 | 1181 | `					   \|\| (ch >= 'a' && ch <= 'z') \|\| (ch >= 'A' && ch <= 'Z')` |
|      4 | 1182 | `					   \|\| (ch >= '0' && ch <= '9')) ){` |
|    ! 0 | 1183 | `						FatalCode("Invalid name for --rf/--rc",1);` |
|    ! 0 | 1184 | `					}` |
|     33 | 1185 | `				}` |
|      7 | 1186 | `				if( strlen(zName) > 250 ){` |
|    ! 0 | 1187 | `					FatalCode("Name too long for --rf/--rc",1);` |
|    ! 0 | 1188 | `				}` |
|      - | 1189 | `				{` |
|      - | 1190 | `					/* Double the namespace separators: inside the snippet's` |
|      - | 1191 | `					 * double-quoted string a lone backslash could form an` |
|      - | 1192 | `					 * escape sequence ("App\name" -> newline). */` |
|      - | 1193 | `					static char zEsc[512];` |
|      7 | 1194 | `					char *pOut = zEsc;` |
|     71 | 1195 | `					for( zChk = zName ; *zChk ; zChk++ ){` |
|     65 | 1196 | `						if( *zChk == '\\' ){` |
|    ! 0 | 1197 | `							*pOut++ = '\\';` |
|    ! 0 | 1198 | `						}` |
|     65 | 1199 | `						*pOut++ = *zChk;` |
|     33 | 1200 | `					}` |
|      7 | 1201 | `					*pOut = 0;` |
|      7 | 1202 | `					snprintf(zReflCode,sizeof(zReflCode),` |
|      - | 1203 | `						"try { echo new %s(\"%s\"), \"\\n\"; } catch (Throwable $e) { echo \"Exception: \", $e->getMessage(), \"\\n\"; exit(1); }",` |
|      - | 1204 | `						zWhat,zEsc);` |
|      - | 1205 | `				}` |
|      7 | 1206 | `				zRunCode = zReflCode;` |
|      7 | 1207 | `				run_code = 1;` |
|     11 | 1208 | `			}else if( strcmp(argv[n], "--re") == 0 ){` |
|      - | 1209 | ``				/* php CLI parity: `--re <extension>` prints the module's whole`` |
|      - | 1210 | `				 * Reflection export. An extension NAME is not an identifier --` |
|      - | 1211 | ``				 * php has `Zend OPcache` and `pdo_sqlite` -- so the charset is`` |
|      - | 1212 | `				 * --rz's, and the refusal is ReflectionExtension's own. */` |
|      - | 1213 | `				static char zExtCode[768];` |
|      - | 1214 | `				const char *zName;` |
|      - | 1215 | `				const char *zChk;` |
|      5 | 1216 | `				if( n + 1 >= argc ){` |
|    ! 0 | 1217 | `					FatalCode("Missing name argument for --re",1);` |
|    ! 0 | 1218 | `				}` |
|      5 | 1219 | `				zName = argv[++n];` |
|     45 | 1220 | `				for( zChk = zName ; *zChk ; zChk++ ){` |
|     41 | 1221 | `					char ch = *zChk;` |
|     41 | 1222 | `					if( !(ch == '_' \|\| ch == ' ' \|\| ch == '.' \|\| ch == '-'` |
|     34 | 1223 | `					   \|\| (ch >= 'a' && ch <= 'z') \|\| (ch >= 'A' && ch <= 'Z')` |
|    ! 0 | 1224 | `					   \|\| (ch >= '0' && ch <= '9')) ){` |
|    ! 0 | 1225 | `						FatalCode("Invalid name for --re",1);` |
|    ! 0 | 1226 | `					}` |
|     21 | 1227 | `				}` |
|      5 | 1228 | `				if( strlen(zName) > 250 ){` |
|    ! 0 | 1229 | `					FatalCode("Name too long for --re",1);` |
|    ! 0 | 1230 | `				}` |
|      5 | 1231 | `				snprintf(zExtCode,sizeof(zExtCode),` |
|      - | 1232 | `					"try { echo new ReflectionExtension(\"%s\"), \"\n\"; }"` |
|      - | 1233 | `					" catch (Throwable $e) { echo \"Exception: \", $e->getMessage(), \"\n\"; exit(1); }",` |
|      - | 1234 | `					zName);` |
|      5 | 1235 | `				zRunCode = zExtCode;` |
|      5 | 1236 | `				run_code = 1;` |
|      5 | 1237 | `			}else if( strcmp(argv[n], "--rz") == 0 ){` |
|      - | 1238 | ``				/* php CLI parity: `--rz <name>` reflects a ZEND extension. PHL`` |
|      - | 1239 | `				 * loads none, so every name is the refusal php prints for one` |
|      - | 1240 | `				 * it does not have -- which is ReflectionZendExtension's own` |
|      - | 1241 | `				 * constructor, reached the way --rf/--rc reach theirs. */` |
|      - | 1242 | `				static char zZendCode[768];` |
|      - | 1243 | `				const char *zName;` |
|      - | 1244 | `				const char *zChk;` |
|      3 | 1245 | `				if( n + 1 >= argc ){` |
|    ! 0 | 1246 | `					FatalCode("Missing name argument for --rz",1);` |
|    ! 0 | 1247 | `				}` |
|      3 | 1248 | `				zName = argv[++n];` |
|     33 | 1249 | `				for( zChk = zName ; *zChk ; zChk++ ){` |
|     31 | 1250 | `					char ch = *zChk;` |
|     31 | 1251 | `					if( !(ch == '_' \|\| ch == ' ' \|\| ch == '.' \|\| ch == '-'` |
|     30 | 1252 | `					   \|\| (ch >= 'a' && ch <= 'z') \|\| (ch >= 'A' && ch <= 'Z')` |
|    ! 0 | 1253 | `					   \|\| (ch >= '0' && ch <= '9')) ){` |
|    ! 0 | 1254 | `						FatalCode("Invalid name for --rz",1);` |
|    ! 0 | 1255 | `					}` |
|     16 | 1256 | `				}` |
|      3 | 1257 | `				if( strlen(zName) > 250 ){` |
|    ! 0 | 1258 | `					FatalCode("Name too long for --rz",1);` |
|    ! 0 | 1259 | `				}` |
|      3 | 1260 | `				snprintf(zZendCode,sizeof(zZendCode),` |
|      - | 1261 | `					"try { echo new ReflectionZendExtension(\"%s\"), \"\n\"; }"` |
|      - | 1262 | `					" catch (Throwable $e) { echo \"Exception: \", $e->getMessage(), \"\n\"; exit(1); }",` |
|      - | 1263 | `					zName);` |
|      3 | 1264 | `				zRunCode = zZendCode;` |
|      3 | 1265 | `				run_code = 1;` |
|      2 | 1266 | `			}else{` |
|      - | 1267 | `				/* Unknown long option */` |
|    ! 0 | 1268 | `				Help();` |
|      - | 1269 | `			}` |
|     18 | 1270 | `			continue;` |
|      - | 1271 | `		}` |
|   2021 | 1272 | `		c = argv[n][1];` |
|   2021 | 1273 | `		if( c == 'b' ){` |
|      - | 1274 | `			/* Dump byte-code instructions */` |
|      3 | 1275 | `			dump_vm = 1;` |
|   2020 | 1276 | `		}else if( c == 'l' ){` |
|      - | 1277 | `			/* Syntax-check only (lint) the file argument that follows */` |
|    358 | 1278 | `			lint_mode = 1;` |
|   1842 | 1279 | `		}else if( c == 'i' ){` |
|      - | 1280 | `			/* Display interpreter information and exit */` |
|      2 | 1281 | `			Info();` |
|   1664 | 1282 | `		}else if( c == 'm' ){` |
|      - | 1283 | ``			/* php CLI parity: `-m` lists the loaded modules, php's two sections`` |
|      - | 1284 | `			 * and its case-insensitive order. PHL loads no Zend extension, so` |
|      - | 1285 | `			 * that section is always the header and nothing under it -- which` |
|      - | 1286 | `			 * is php's own shape for a build with none. Rides the -r code path` |
|      - | 1287 | `			 * the way --rf/--rc do. */` |
|      3 | 1288 | `			zRunCode =` |
|      - | 1289 | `				"$m = get_loaded_extensions();"` |
|      - | 1290 | `				"usort($m, 'strcasecmp');"` |
|      - | 1291 | `				"echo \"[PHP Modules]\n\";"` |
|      - | 1292 | `				"foreach ($m as $x) { echo $x, \"\n\"; }"` |
|      - | 1293 | `				"echo \"\n[Zend Modules]\n\";"` |
|      - | 1294 | `				"foreach (get_loaded_extensions(true) as $x) { echo $x, \"\n\"; }"` |
|      - | 1295 | `				"echo \"\n\";";` |
|      3 | 1296 | `			run_code = 1;` |
|   1662 | 1297 | `		}else if( c == 'f' ){` |
|      - | 1298 | ``			/* php CLI parity: `-f <file>` explicitly names the script to run.`` |
|      - | 1299 | `			 * The path follows as the next argument, which the positional` |
|      - | 1300 | `			 * file handling below already consumes, so treat -f as a no-op. */` |
|    ! 0 | 1301 | `			continue;` |
|   1661 | 1302 | `		}else if( c == 'r' ){` |
|      - | 1303 | `			/* Run inline PHP code from next argument (php -r style) */` |
|    570 | 1304 | `			if( n + 1 >= argc ){` |
|      - | 1305 | `				/* Missing code argument */` |
|    ! 0 | 1306 | `				FatalCode("Missing code argument for -r",1);` |
|    ! 0 | 1307 | `			}` |
|    570 | 1308 | `			zRunCode = argv[++n];` |
|    570 | 1309 | `			run_code = 1;` |
|   1377 | 1310 | `		}else if( c == 'S' ){` |
|      - | 1311 | `			/* Start built-in development server */` |
|      - | 1312 | `#ifdef PHL_ENABLE_SERVER` |
|     32 | 1313 | `			if( n + 1 >= argc ){` |
|    ! 0 | 1314 | `				FatalCode("Missing host:port argument for -S",1);` |
|    ! 0 | 1315 | `			}` |
|     32 | 1316 | `			zServerAddr = argv[++n];` |
|     32 | 1317 | `			server_mode = 1;` |
|      - | 1318 | `#else` |
|      - | 1319 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|      - | 1320 | `#endif` |
|   1077 | 1321 | `		}else if( c == 't' ){` |
|      - | 1322 | `			/* Set document root for the server */` |
|      - | 1323 | `#ifdef PHL_ENABLE_SERVER` |
|     32 | 1324 | `			if( n + 1 >= argc ){` |
|    ! 0 | 1325 | `				FatalCode("Missing docroot argument for -t",1);` |
|    ! 0 | 1326 | `			}` |
|     32 | 1327 | `			zDocRoot = argv[++n];` |
|      - | 1328 | `#else` |
|      - | 1329 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|      - | 1330 | `#endif` |
|   1045 | 1331 | `		}else if( c == 'v' ){` |
|      - | 1332 | `			/* Display version */` |
|    ! 0 | 1333 | `			Version();` |
|   1029 | 1334 | `		}else if( c == 'd' ){` |
|      - | 1335 | `			/* php CLI parity: -d name=value defines a php.ini entry` |
|      - | 1336 | `			 * (repeatable; applied to the VM after compile, in order). */` |
|    509 | 1337 | `			if( n + 1 >= argc ){` |
|    ! 0 | 1338 | `				FatalCode("Missing name=value argument for -d",1);` |
|    ! 0 | 1339 | `			}` |
|    509 | 1340 | `			if( nIniDefine < (int)(sizeof(azIniDefine)/sizeof(azIniDefine[0])) ){` |
|    509 | 1341 | `				azIniDefine[nIniDefine++] = argv[++n];` |
|    256 | 1342 | `			}else{` |
|    ! 0 | 1343 | `				FatalCode("Too many -d directives",1);` |
|      4 | 1344 | `			}` |
|    772 | 1345 | `		}else if( c == 'c' ){` |
|      - | 1346 | `			/* php CLI parity: -c file loads php.ini directives from a file` |
|      - | 1347 | `			 * (name=value lines; [sections] and ;/# comments ignored). */` |
|    520 | 1348 | `			if( n + 1 >= argc ){` |
|    ! 0 | 1349 | `				FatalCode("Missing file argument for -c",1);` |
|    ! 0 | 1350 | `			}` |
|    520 | 1351 | `			zIniFile = argv[++n];` |
|    260 | 1352 | `		}else{` |
|      - | 1353 | `			/* Display a help message and exit */` |
|    ! 0 | 1354 | `			Help();` |
|      - | 1355 | `		}` |
|   1012 | 1356 | `	}` |
|      - | 1357 | `#ifdef PHL_ENABLE_SERVER` |
|   7857 | 1358 | `	if( server_mode ){` |
|      - | 1359 | `		/* Parse host:port from zServerAddr */` |
|      - | 1360 | `		char zHost[256];` |
|     32 | 1361 | `		int iPort = 0;` |
|      - | 1362 | `		const char *zColon;` |
|     32 | 1363 | `		const char *zRouter = 0;` |
|     32 | 1364 | `		zColon = strrchr(zServerAddr, ':');` |
|     32 | 1365 | `		if( zColon == 0 ){` |
|    ! 0 | 1366 | `			FatalCode("Invalid address format. Use host:port (e.g., localhost:8080)",1);` |
|    ! 0 | 1367 | `		}` |
|      - | 1368 | `		{` |
|     32 | 1369 | `			int nHostLen = (int)(zColon - zServerAddr);` |
|     32 | 1370 | `			if( nHostLen >= (int)sizeof(zHost) ) nHostLen = (int)sizeof(zHost) - 1;` |
|     32 | 1371 | `			memcpy(zHost, zServerAddr, nHostLen);` |
|     32 | 1372 | `			zHost[nHostLen] = 0;` |
|      - | 1373 | `		}` |
|     32 | 1374 | `		iPort = atoi(zColon + 1);` |
|     32 | 1375 | `		if( iPort <= 0 \|\| iPort > 65535 ){` |
|    ! 0 | 1376 | `			FatalCode("Invalid port number",1);` |
|    ! 0 | 1377 | `		}` |
|      - | 1378 | `		/* Check for optional router script */` |
|     32 | 1379 | `		if( n < argc ){` |
|    ! 0 | 1380 | `			zRouter = argv[n];` |
|    ! 0 | 1381 | `		}` |
|     32 | 1382 | `		return phl_serve(zHost, iPort, zDocRoot, zRouter, PHL_ResolveBinaryPath(argv[0]));` |
|      - | 1383 | `	}` |
|      - | 1384 | `#endif` |
|   7825 | 1385 | `	if( (n >= argc \|\| dash_dash) && !run_code ){` |
|      - | 1386 | ``		/* No file and no -r: php reads the script from stdin. `--` forces this`` |
|      - | 1387 | `		 * (rest are script args); otherwise only when stdin is a pipe/redirect` |
|      - | 1388 | `		 * (an interactive terminal would just block). */` |
|     12 | 1389 | `		if( dash_dash \|\| PHL_StdinIsPipe() ){` |
|     12 | 1390 | `			zStdinCode = PHL_SlurpStdin(&nStdinCode);` |
|     12 | 1391 | `			if( zStdinCode == 0 ){` |
|    ! 0 | 1392 | `				FatalCode("IO error while reading standard input",1);` |
|    ! 0 | 1393 | `			}` |
|     12 | 1394 | `			stdin_code = 1;` |
|      6 | 1395 | `		}else{` |
|    ! 0 | 1396 | `			puts("Missing PHP file to compile");` |
|    ! 0 | 1397 | `			Help();` |
|      - | 1398 | `		}` |
|      6 | 1399 | `	}` |
|      - | 1400 |  |
|      - | 1401 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|      - | 1402 | `	/* Install an unhandled exception minidump handler for Windows debug builds */` |
|      5 | 1403 | `	CreateMiniDumpOnUnHandledException();` |
|      - | 1404 | `#endif` |
|      - | 1405 | `	/* Allocate a new PH7 engine instance */` |
|   7401 | 1406 | `	rc = ph7_init(&pEngine);` |
|   7401 | 1407 | `	pFatalEngine = pEngine;` |
|   7401 | 1408 | `	if( rc != PH7_OK ){` |
|      - | 1409 | `		/*` |
|      - | 1410 | `		 * If the supplied memory subsystem is so sick that we are unable` |
|      - | 1411 | `		 * to allocate a tiny chunk of memory,there is no much we can do here.` |
|      - | 1412 | `		 */` |
|    ! 0 | 1413 | `		Fatal("Error while allocating a new PH7 engine instance");` |
|    ! 0 | 1414 | `	}` |
|      - | 1415 | `	/* Compile-time diagnostics go to STDERR, where php's log copy goes. This used` |
|      - | 1416 | ``	 * to be Output_Consumer, i.e. STDOUT -- so `phl -l bad.php` printed the parse`` |
|      - | 1417 | `	 * error into program output, and anything capturing a script's stdout read` |
|      - | 1418 | `	 * php's stderr text as data. It is a FALLBACK channel: once the VM's own` |
|      - | 1419 | `	 * streams are wired below, the engine routes each copy to the right one, and` |
|      - | 1420 | `	 * this only serves the window in which the main script's own compile creates` |
|      - | 1421 | `	 * the VM. */` |
|   7401 | 1422 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,` |
|      - | 1423 | `		Error_Consumer, /* Compile-time diagnostic consumer (STDERR) */` |
|      - | 1424 | `		0 /* NULL: Callback Private data */` |
|      - | 1425 | `		);` |
|      - | 1426 | `	/* ...and the program-output stream the DISPLAY copy of that same diagnostic` |
|      - | 1427 | `	 * takes, which php puts on STDOUT. Also a fallback: the VM's own output` |
|      - | 1428 | `	 * consumer is installed below, and does not exist while the main script is` |
|      - | 1429 | `	 * being compiled. */` |
|   7401 | 1430 | `	ph7_config(pEngine,PH7_CONFIG_OUTPUT,` |
|      - | 1431 | `		Output_Consumer, /* Compile-time DISPLAY copy consumer (STDOUT) */` |
|      - | 1432 | `		0 /* NULL: Callback Private data */` |
|      - | 1433 | `		);` |
|      - | 1434 | `	/* Report script run-time errors (now default behavior), then the php.ini` |
|      - | 1435 | `	 * directives, and all of it BEFORE anything is compiled -- which is where php` |
|      - | 1436 | `	 * reads php.ini. These used to be applied to the finished VM, i.e. after the` |
|      - | 1437 | `	 * main script's own compile had already raised every diagnostic it was going` |
|      - | 1438 | ``	 * to: `-d display_errors=1` never moved a parse error onto stdout, and`` |
|      - | 1439 | ``	 * `-d log_errors=0` never took one off stderr, whatever php did. The -c file`` |
|      - | 1440 | `	 * comes first and -d overrides it in CLI order (php's precedence), and the` |
|      - | 1441 | `` 	 * whole queue lands after the error-report default so `-d error_reporting=0` `` |
|      - | 1442 | `	 * can still lower it. */` |
|   7401 | 1443 | `	ph7_config(pEngine,PH7_CONFIG_ERR_REPORT);` |
|   7401 | 1444 | `	if( zIniFile ){` |
|    520 | 1445 | `		PHL_LoadIniFile(pEngine,zIniFile);` |
|    260 | 1446 | `	}` |
|   7401 | 1447 | `	if( nIniDefine > 0 ){` |
|      - | 1448 | `		/* php's CLI SAPI joins every -d into ONE buffer for zend_parse_ini_string(),` |
|      - | 1449 | `		 * prefixed by five lines of its own hardcoded startup ini (php_cli.c's` |
|      - | 1450 | `		 * HARDCODED_INI: html_errors, implicit_flush, output_buffering,` |
|      - | 1451 | `		 * max_execution_time, max_input_time) -- so the first -d is on line 6 and` |
|      - | 1452 | `		 * a directive's line, in the "on line N" a refusal warns about, is 5 plus` |
|      - | 1453 | `		 * its 1-based position among every -d php was given.` |
|      - | 1454 | `		 *` |
|      - | 1455 | `		 * Join them the same way rather than applying each on its own: they are` |
|      - | 1456 | `		 * ONE scanner input, so a quoted run one -d opens keeps going into the` |
|      - | 1457 | `` 		 * next one's text exactly as it would inside a file. `-d "x='a" -d "y=b'"` `` |
|      - | 1458 | ``		 * is a single raw string spanning both, and an unterminated `"` in the`` |
|      - | 1459 | `		 * first swallows every -d behind it. */` |
|    295 | 1460 | `		size_t nCap = 0, nUsed = 0;` |
|      - | 1461 | `		char *zBuf;` |
|      - | 1462 | `		int i;` |
|    800 | 1463 | `		for( i = 0 ; i < nIniDefine ; i++ ){` |
|    509 | 1464 | `			nCap += strlen(azIniDefine[i]) + sizeof("\"\"=1\n");` |
|    256 | 1465 | `		}` |
|    295 | 1466 | `		zBuf = (char *)malloc(nCap);` |
|    295 | 1467 | `		if( zBuf ){` |
|    800 | 1468 | `			for( i = 0 ; i < nIniDefine ; i++ ){` |
|    509 | 1469 | `				nUsed += PHL_IniDefineLine(azIniDefine[i],&zBuf[nUsed]);` |
|    256 | 1470 | `			}` |
|    295 | 1471 | `			PHL_ScanIniSource(pEngine,zBuf,nUsed,"Unknown",6);` |
|    295 | 1472 | `			free(zBuf);` |
|    145 | 1473 | `		}` |
|    145 | 1474 | `	}` |
|      - | 1475 | `	/* Optional per-allocation memory cap (PHL_MAX_ALLOC=bytes). Used to` |
|      - | 1476 | `	 * deterministically exercise out-of-memory paths (see tests/ph7/003-stress).` |
|      - | 1477 | `	 * Clamp to a floor above the pool bucket size (SXMEM_POOL_MAXALLOC, 32 KB)` |
|      - | 1478 | `	 * so the engine can still start; VMs inherit it at creation. */` |
|      - | 1479 | `	{` |
|      - | 1480 | `		unsigned long uMax;` |
|      - | 1481 | `		/* floor: keep above the pool bucket size; clamp: nMaxRequest is 32-bit */` |
|   7401 | 1482 | `		if( PHL_EnvULong("PHL_MAX_ALLOC",65536UL,0xFFFFFFFFUL,&uMax) ){` |
|    ! 0 | 1483 | `			ph7_config(pEngine,PH7_CONFIG_MAX_ALLOC,(unsigned int)uMax);` |
|    ! 0 | 1484 | `		}` |
|      - | 1485 | `	}` |
|      - | 1486 | `	/* Optional per-input byte cap (PHL_MAX_INPUT=bytes). Used to exercise the` |
|      - | 1487 | `	 * input-size rejection path at a manageable scale (see tests/ph7/003-stress). */` |
|      - | 1488 | `	{` |
|      - | 1489 | `		unsigned long uMax;` |
|   7401 | 1490 | `		if( PHL_EnvULong("PHL_MAX_INPUT",1UL,0xFFFFFFFFUL,&uMax) ){` |
|    ! 0 | 1491 | `			ph7_config(pEngine,PH7_CONFIG_MAX_INPUT,(unsigned int)uMax);` |
|    ! 0 | 1492 | `		}` |
|      - | 1493 | `	}` |
|      - | 1494 | `	/* Syntax-check only mode (-l): compile the target file, print PHP's summary` |
|      - | 1495 | `	 * line and exit without executing. The error consumer installed above` |
|      - | 1496 | `	 * already prints any parse error; ph7_compile_file leaves *pVm NULL on a` |
|      - | 1497 | `	 * compile/IO error, so only a successful compile owns a VM to release. */` |
|   7401 | 1498 | `	if( lint_mode ){` |
|      - | 1499 | `		const char *zFile;` |
|    358 | 1500 | `		if( n >= argc ){` |
|      - | 1501 | ``			/* No file argument (e.g. `-l` alone, or `-l` mixed with `-r`). */`` |
|    ! 0 | 1502 | `			ph7_release(pEngine);` |
|    ! 0 | 1503 | `			pFatalEngine = 0;` |
|    ! 0 | 1504 | `			puts("No input file specified");` |
|    ! 0 | 1505 | `			return 255;` |
|      - | 1506 | `		}` |
|    358 | 1507 | `		zFile = argv[n];` |
|    358 | 1508 | `		rc = ph7_compile_file(pEngine,zFile,&pVm,PH7_SYNTAX_CHECK);` |
|    358 | 1509 | `		if( rc == PH7_OK ){` |
|    130 | 1510 | `			printf("No syntax errors detected in %s\n",zFile);` |
|    130 | 1511 | `			ph7_vm_release(pVm);` |
|    294 | 1512 | `		}else if( rc == PH7_IO_ERR ){` |
|      - | 1513 | `			/* php says this on STDERR, like the run path above */` |
|      4 | 1514 | `			fprintf(stderr,"Could not open input file: %s\n",zFile);` |
|      2 | 1515 | `		}else{` |
|    227 | 1516 | `			printf("Errors parsing %s\n",zFile);` |
|      - | 1517 | `		}` |
|    358 | 1518 | `		ph7_release(pEngine);` |
|    358 | 1519 | `		pFatalEngine = 0;` |
|      - | 1520 | `		/* php's two exit codes are not one: a file it cannot OPEN is 1 (a bad` |
|      - | 1521 | `		 * invocation), a file that does not PARSE is 255. */` |
|    358 | 1522 | `		if( rc == PH7_OK ){` |
|    130 | 1523 | `			return 0;` |
|      - | 1524 | `		}` |
|    231 | 1525 | `		return (rc == PH7_IO_ERR) ? 1 : 255;` |
|      - | 1526 | `	}` |
|      - | 1527 | `	/* Now,it's time to compile our PHP file */` |
|   7047 | 1528 | `	if( run_code ){` |
|      - | 1529 | `		/* Compile inline PHP code string (PHP only - no tags needed) */` |
|    585 | 1530 | `		rc = ph7_compile_v2(` |
|    291 | 1531 | `			pEngine, /* PH7 Engine */` |
|    291 | 1532 | `			zRunCode, /* Source code */` |
|      - | 1533 | `			-1,       /* Let API compute length */` |
|      - | 1534 | `			&pVm,     /* OUT: Compiled PHP program */` |
|      - | 1535 | `			PH7_PHP_ONLY /* Inline PHP, no tags expected */` |
|      - | 1536 | `			);` |
|    585 | 1537 | `		if( rc != PH7_OK ){ /* Compile error */` |
|      7 | 1538 | `			if( rc == PH7_VM_ERR ){` |
|    ! 0 | 1539 | `				Fatal("VM initialization error");` |
|    ! 0 | 1540 | `			}else{` |
|      - | 1541 | `				/* Compile-time error. The diagnostic has already been printed by the` |
|      - | 1542 | `				 * error consumer; php adds nothing else, it just exits 255. */` |
|      7 | 1543 | `				FatalSilent();` |
|      - | 1544 | `			}` |
|      6 | 1545 | `		}` |
|   6756 | 1546 | `	}else if( stdin_code ){` |
|      - | 1547 | `		/* Script read from stdin: compile it like a file (PHP tags expected). */` |
|     12 | 1548 | `		rc = ph7_compile_v2(` |
|      6 | 1549 | `			pEngine,     /* PH7 Engine */` |
|      6 | 1550 | `			zStdinCode,  /* Source code slurped from stdin */` |
|      6 | 1551 | `			nStdinCode,  /* Its byte length */` |
|      - | 1552 | `			&pVm,        /* OUT: Compiled PHP program */` |
|      - | 1553 | `			0            /* IN: tag mode, like a file */` |
|      - | 1554 | `			);` |
|     12 | 1555 | `		if( rc != PH7_OK ){ /* Compile error */` |
|    ! 0 | 1556 | `			if( rc == PH7_VM_ERR ){` |
|    ! 0 | 1557 | `				Fatal("VM initialization error");` |
|    ! 0 | 1558 | `			}else{` |
|    ! 0 | 1559 | `				FatalSilent();` |
|      - | 1560 | `			}` |
|    ! 0 | 1561 | `		}` |
|      6 | 1562 | `	}else{` |
|   6453 | 1563 | `		rc = ph7_compile_file(` |
|   2965 | 1564 | `			pEngine, /* PH7 Engine */` |
|   6448 | 1565 | `			argv[n], /* Path to the PHP file to compile */` |
|      - | 1566 | `			&pVm,    /* OUT: Compiled PHP program */` |
|      - | 1567 | `			0        /* IN: Compile flags */` |
|      - | 1568 | `			);` |
|   6453 | 1569 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   1018 | 1570 | `			if( rc == PH7_IO_ERR ){` |
|      - | 1571 | `				/* php names the file it could not open, and says so on STDERR --` |
|      - | 1572 | `				 * this answered a generic "IO error while opening the target file"` |
|      - | 1573 | `				 * on stdout, which a script capturing program output then read as` |
|      - | 1574 | `				 * part of the answer. */` |
|      4 | 1575 | `				fprintf(stderr,"Could not open input file: %s\n",argv[n]);` |
|      4 | 1576 | `				FatalSilentCode(1);` |
|   1016 | 1577 | `			}else if( rc == PH7_VM_ERR ){` |
|    ! 0 | 1578 | `				Fatal("VM initialization error");` |
|    ! 0 | 1579 | `			}else{` |
|      - | 1580 | `				/* Compile-time error. The diagnostic has already been printed by the` |
|      - | 1581 | `				 * error consumer; php prints nothing further and exits 255. */` |
|   1014 | 1582 | `				FatalSilent();` |
|      - | 1583 | `			}` |
|    507 | 1584 | `		}` |
|      - | 1585 | `	}` |
|      - | 1586 | `	/*` |
|      - | 1587 | `	 * Now we have our script compiled,it's time to configure our VM.` |
|      - | 1588 | `	 * We will install the VM output consumer callback defined above` |
|      - | 1589 | `	 * so that we can consume the VM output and redirect it to STDOUT.` |
|      - | 1590 | `	 */` |
|   6537 | 1591 | `	rc = ph7_vm_config(pVm,` |
|      - | 1592 | `		PH7_VM_CONFIG_OUTPUT,` |
|      - | 1593 | `		Output_Consumer,    /* Output Consumer callback */` |
|      - | 1594 | `		0                   /* Callback private data */` |
|      - | 1595 | `		);` |
|   6537 | 1596 | `	if( rc != PH7_OK ){` |
|    ! 0 | 1597 | `		Fatal("Error while installing the VM output consumer callback");` |
|    ! 0 | 1598 | `	}` |
|      - | 1599 | `	/* Diagnostics stream: route the log copy of runtime warnings/notices and the` |
|      - | 1600 | `	 * uncaught-exception fatal to STDERR (gated by log_errors), so program STDOUT` |
|      - | 1601 | `	 * stays clean like stock CLI php. */` |
|   6537 | 1602 | `	rc = ph7_vm_config(pVm,` |
|      - | 1603 | `		PH7_VM_CONFIG_ERR_STREAM,` |
|      - | 1604 | `		Error_Consumer,     /* Diagnostics (STDERR) consumer callback */` |
|      - | 1605 | `		0                   /* Callback private data */` |
|      - | 1606 | `		);` |
|   6537 | 1607 | `	if( rc != PH7_OK ){` |
|    ! 0 | 1608 | `		Fatal("Error while installing the VM diagnostics consumer callback");` |
|    ! 0 | 1609 | `	}` |
|      - | 1610 | `	/* Optional recursion caps via the environment (like PHL_MAX_ALLOC). The host` |
|      - | 1611 | `	 * defaults are PHP-parity — PHP call depth is UNBOUNDED (heap-bound) and only` |
|      - | 1612 | `	 * the native VmByteCodeExec nesting is capped — so these knobs are for tests` |
|      - | 1613 | `	 * and embedders that want a tighter bound, not to raise a low default.` |
|      - | 1614 | `	 *   PHL_MAX_RECURSION   -> PH7_VM_CONFIG_RECURSION_DEPTH (PHP call depth; any` |
|      - | 1615 | `	 *                          positive value is a cap, PHL_EnvULong rejects 0)` |
|      - | 1616 | `	 *   PHL_MAX_NATIVE_DEPTH -> PH7_VM_CONFIG_NATIVE_DEPTH   (native nesting;` |
|      - | 1617 | `	 *                          floor 2) */` |
|      - | 1618 | `	{` |
|      - | 1619 | `		unsigned long uMax;` |
|   6537 | 1620 | `		if( PHL_EnvULong("PHL_MAX_RECURSION",1UL,0x7FFFFFFFUL,&uMax) ){` |
|      5 | 1621 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_RECURSION_DEPTH,(int)uMax);` |
|      2 | 1622 | `		}` |
|   6537 | 1623 | `		if( PHL_EnvULong("PHL_MAX_NATIVE_DEPTH",2UL,0x7FFFFFFFUL,&uMax) ){` |
|     12 | 1624 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_NATIVE_DEPTH,(int)uMax);` |
|      5 | 1625 | `		}` |
|      - | 1626 | `	}` |
|      - | 1627 | `	/* Define PHP_BINARY: absolute path of this interpreter */` |
|   9799 | 1628 | `	ph7_create_constant(pVm,"PHP_BINARY",PHL_PhpBinaryConst,` |
|   6532 | 1629 | `		(void *)PHL_ResolveBinaryPath(argv[0]));` |
|      - | 1630 | `	/* Register the script arguments as $argv[] plus the matching $argc count and` |
|      - | 1631 | `	 * the CLI $_SERVER entries, matching PHP: $argv[0] is the script path (file` |
|      - | 1632 | `	 * mode) or the literal "Standard input code" (-r mode), followed by the` |
|      - | 1633 | `	 * script's own arguments.` |
|      - | 1634 | `	 */` |
|      - | 1635 | `	{` |
|   6537 | 1636 | `		const char *zScriptName = (run_code \|\| stdin_code) ? "Standard input code" : argv[n];` |
|   6537 | 1637 | `		int argv_count = 0;` |
|      - | 1638 | `		ph7_value *pArgc;` |
|      - | 1639 | `		/* Count only the entries actually inserted, so $argc can never disagree` |
|      - | 1640 | `		 * with count($argv) if a registration fails. */` |
|   6537 | 1641 | `		if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,zScriptName) == PH7_OK ){` |
|   6534 | 1642 | `			argv_count++;` |
|   3259 | 1643 | `		}` |
|      - | 1644 | `		/* The script's own arguments follow: in file mode argv[n] is the script` |
|      - | 1645 | `		 * (registered above), so they start at n+1; in -r mode they start at n. */` |
|   6613 | 1646 | `		for( n = (run_code \|\| stdin_code) ? n : n + 1; n < argc ; ++n ){` |
|     81 | 1647 | `			if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,argv[n]) == PH7_OK ){` |
|     81 | 1648 | `				argv_count++;` |
|     38 | 1649 | `			}` |
|     43 | 1650 | `		}` |
|      - | 1651 | `		/* $argc: a plain integer global equal to count($argv). */` |
|   6537 | 1652 | `		pArgc = ph7_new_scalar(pVm);` |
|   6537 | 1653 | `		if( pArgc ){` |
|   6534 | 1654 | `			ph7_value_int(pArgc,argv_count);` |
|   6534 | 1655 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_CREATE_VAR,"argc",pArgc);` |
|   6534 | 1656 | `			ph7_release_value(pVm,pArgc);` |
|   3259 | 1657 | `		}` |
|      - | 1658 | `		/* Mirror $argv/$argc into $_SERVER['argv']/$_SERVER['argc'] (php CLI). */` |
|   6537 | 1659 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ARGV);` |
|      - | 1660 | `		/* $_SERVER entries frameworks read at CLI bootstrap. SCRIPT_FILENAME is` |
|      - | 1661 | `		 * already set to the script path by PH7_HashmapCreateSuper. */` |
|   6537 | 1662 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"SCRIPT_NAME",zScriptName,-1);` |
|   6537 | 1663 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PHP_SELF",zScriptName,-1);` |
|   6537 | 1664 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"DOCUMENT_ROOT","",0);` |
|      - | 1665 | `		{` |
|      - | 1666 | `			char zTime[32];` |
|   6537 | 1667 | `			snprintf(zTime,sizeof(zTime),"%ld",(long)time(0));` |
|   6537 | 1668 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"REQUEST_TIME",zTime,-1);` |
|      - | 1669 | `		}` |
|      - | 1670 | `#ifndef __WINNT__` |
|      - | 1671 | `		{` |
|      - | 1672 | `			char zCwd[PATH_MAX];` |
|   6532 | 1673 | `			if( getcwd(zCwd,sizeof(zCwd)) ){` |
|   6529 | 1674 | `				ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PWD",zCwd,-1);` |
|   3259 | 1675 | `			}` |
|      - | 1676 | `		}` |
|      - | 1677 | `#endif` |
|      - | 1678 | `	}` |
|   6537 | 1679 | `	if( dump_vm ){` |
|      - | 1680 | `		/* Dump PH7 byte-code instructions */` |
|      3 | 1681 | `		ph7_vm_dump_v2(pVm,` |
|      - | 1682 | `			Output_Consumer, /* Dump consumer callback */` |
|      - | 1683 | `			0` |
|      - | 1684 | `			);` |
|      1 | 1685 | `	}` |
|      - | 1686 | `	/*` |
|      - | 1687 | `	 * And finally, execute our program. Note that your output (STDOUT in our case)` |
|      - | 1688 | `	 * should display the result.` |
|      - | 1689 | `	 */` |
|      - | 1690 | `	{` |
|   6537 | 1691 | `		int iExitStatus = 0;` |
|   6537 | 1692 | `		ph7_vm_exec(pVm,&iExitStatus);` |
|      - | 1693 | `		/* All done, cleanup the mess left behind.` |
|      - | 1694 | `		*/` |
|   6545 | 1695 | `		ph7_vm_release(pVm);` |
|   6545 | 1696 | `		ph7_release(pEngine);` |
|   6545 | 1697 | `		pFatalEngine = 0;` |
|      - | 1698 | `		/* The stdin slurp outlives compilation (the compiler keeps pointers` |
|      - | 1699 | `		 * into the source text), so it is freed only here, after the VM. */` |
|   6545 | 1700 | `		if( zStdinCode ){` |
|     12 | 1701 | `			free(zStdinCode);` |
|      6 | 1702 | `		}` |
|      - | 1703 | `		/* Propagate the script exit status (set via exit()/die()) */` |
|   6545 | 1704 | `		return iExitStatus;` |
|      - | 1705 | `	}` |
|   3460 | 1706 | `}` |
|      - | 1707 |  |
