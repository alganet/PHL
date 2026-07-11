# src/phl/phl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 199/253 lines (78.66%)

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
|     - |   34 | `/* Make sure this header file is available.*/` |
|     - |   35 | `#include "ph7.h"` |
|     - |   36 | `#ifdef PHL_ENABLE_SERVER` |
|     - |   37 | `#include "server.h"` |
|     - |   38 | `#endif` |
|     - |   39 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|     - |   40 | `#define MINIDUMP_IMPLEMENTATION` |
|     - |   41 | `#include "minidump.h"` |
|     - |   42 | `#endif` |
|     - |   43 | `/*` |
|     - |   44 | ` * Display an error message and exit.` |
|     - |   45 | ` */` |
|   372 |   46 | `static void FatalCode(const char *zMsg,int iCode)` |
|     4 |   47 | `{` |
|   376 |   48 | `	puts(zMsg);` |
|     - |   49 | `	/* Shutdown the library */` |
|   376 |   50 | `	ph7_lib_shutdown();` |
|     - |   51 | `	/* Exit immediately */` |
|   376 |   52 | `	exit(iCode);` |
|   ! 0 |   53 | `}` |
|     - |   54 | `/*` |
|     - |   55 | ` * php-parity default: fatal engine/compile failures exit 255 (php exits 255` |
|     - |   56 | ` * on a fatal compile error); usage and IO errors use FatalCode(msg, 1)` |
|     - |   57 | ` * directly, mirroring php's exit 1 for bad invocations / unopenable input.` |
|     - |   58 | ` */` |
|   372 |   59 | `static void Fatal(const char *zMsg)` |
|     4 |   60 | `{` |
|   376 |   61 | `	FatalCode(zMsg,255);` |
|   186 |   62 | `}` |
|     - |   63 | `/*` |
|     - |   64 | ` * Display the banner,a help message and exit.` |
|     - |   65 | ` */` |
|     2 |   66 | `static void Help(void)` |
|     1 |   67 | `{` |
|     3 |   68 | `	puts("phl [-h\|--help\|-b\|-i\|-l\|-v\|--version\|-r code] path/to/php_file [script args]");` |
|     - |   69 | `#ifdef PHL_ENABLE_SERVER` |
|     3 |   70 | `	puts("phl -S host:port [-t docroot] [router.php]");` |
|     - |   71 | `#endif` |
|     3 |   72 | `	puts("\t-b: Dump PH7 byte-code instructions");` |
|     3 |   73 | `	puts("\t-i: Display interpreter information and exit");` |
|     3 |   74 | `	puts("\t-l: Syntax-check (lint) the given file and exit");` |
|     3 |   75 | `	puts("\t-r code: Run code from command line (no tags needed)");` |
|     - |   76 | `#ifdef PHL_ENABLE_SERVER` |
|     3 |   77 | `	puts("\t-S host:port: Start the built-in development server");` |
|     3 |   78 | `	puts("\t-t docroot: Document root for the server (default: current directory)");` |
|     - |   79 | `#endif` |
|     3 |   80 | `	puts("\t-v, --version: Display version information and exit");` |
|     3 |   81 | `	puts("\t-h, --help: Display this message and exit");` |
|     - |   82 | `	/* Exit immediately */` |
|     3 |   83 | `	exit(0);` |
|   ! 0 |   84 | `}` |
|     - |   85 | `/*` |
|     - |   86 | ` * Display version information and exit.` |
|     - |   87 | ` */` |
|     6 |   88 | `static void Version(void)` |
|     1 |   89 | `{` |
|     7 |   90 | `	puts("PHL " PH7_VERSION " (cli) (built " __DATE__ " " __TIME__ ")");` |
|     7 |   91 | `	puts("Copyright (c) 2011-2014 Symisc Systems, 2025 Alexandre Gomes Gaigalas");` |
|     - |   92 | `	/* Exit immediately */` |
|     7 |   93 | `	exit(0);` |
|   ! 0 |   94 | `}` |
|     - |   95 | `/*` |
|     - |   96 | ` * Display interpreter information (php -i) and exit. PHP's CLI -i is plain text` |
|     - |   97 | ` * (the phpinfo() builtin emits HTML, suited to the web SAPI), so this prints a` |
|     - |   98 | ` * concise curated subset on the terminal rather than reusing that builtin.` |
|     - |   99 | ` */` |
|     2 |  100 | `static void Info(void)` |
|   ! 0 |  101 | `{` |
|     2 |  102 | `	printf("phpinfo()\n");` |
|     2 |  103 | `	printf("PHP Version => %s\n\n", PHP_COMPAT_VERSION);` |
|     2 |  104 | `	printf("System => %s\n",` |
|     - |  105 | `#ifdef __WINNT__` |
|     - |  106 | `		"Windows NT"` |
|     - |  107 | `#elif defined(__UNIXES__)` |
|     - |  108 | `		"UNIX-Like"` |
|     - |  109 | `#else` |
|     - |  110 | `		"Other OS"` |
|     - |  111 | `#endif` |
|     - |  112 | `	);` |
|     2 |  113 | `	printf("Build Date => %s %s\n", __DATE__, __TIME__);` |
|     2 |  114 | `	printf("PHL Version => %s\n", PH7_VERSION);` |
|     2 |  115 | `	printf("PHP SAPI => cli\n");` |
|     - |  116 | `	/* Exit immediately */` |
|     2 |  117 | `	exit(0);` |
|   ! 0 |  118 | `}` |
|     - |  119 | `#ifdef __WINNT__` |
|     - |  120 | `#include <Windows.h>` |
|     - |  121 | `#else` |
|     - |  122 | `/* Assume UNIX */` |
|     - |  123 | `#include <unistd.h>` |
|     - |  124 | `#include <limits.h>` |
|     - |  125 | `#endif` |
|     - |  126 | `/*` |
|     - |  127 | ` * The following define is used by the UNIX built and have` |
|     - |  128 | ` * no particular meaning on windows.` |
|     - |  129 | ` */` |
|     - |  130 | `#ifndef STDOUT_FILENO` |
|     - |  131 | `#define STDOUT_FILENO	1` |
|     - |  132 | `#endif` |
|     - |  133 | `#ifndef PATH_MAX` |
|     - |  134 | `#define PATH_MAX 4096` |
|     - |  135 | `#endif` |
|     - |  136 | `static char zPhlBinaryPath[PATH_MAX];` |
|     - |  137 | `/*` |
|     - |  138 | ` * Expand callback for the PHP_BINARY constant.` |
|     - |  139 | ` * pUserData points to the resolved binary path.` |
|     - |  140 | ` */` |
|     2 |  141 | `static void PHL_PhpBinaryConst(ph7_value *pVal,void *pUserData)` |
|     1 |  142 | `{` |
|     3 |  143 | `	ph7_value_string(pVal,(const char *)pUserData,-1);` |
|     3 |  144 | `}` |
|     - |  145 | `/*` |
|     - |  146 | ` * Resolve the absolute path of the running interpreter.` |
|     - |  147 | ` * Falls back to argv[0] verbatim (e.g. bare PATH invocation):` |
|     - |  148 | ` * consumers spawning it again go through the shell, which re-resolves it.` |
|     - |  149 | ` */` |
|  3456 |  150 | `static const char * PHL_ResolveBinaryPath(const char *zArgv0)` |
|     5 |  151 | `{` |
|     - |  152 | `#ifdef __WINNT__` |
|     5 |  153 | `	DWORD nLen = GetModuleFileNameA(0,zPhlBinaryPath,(DWORD)sizeof(zPhlBinaryPath));` |
|     5 |  154 | `	if( nLen > 0 && nLen < sizeof(zPhlBinaryPath) ){` |
|     5 |  155 | `		return zPhlBinaryPath;` |
|     - |  156 | `	}` |
|     - |  157 | `#else` |
|  3456 |  158 | `	if( realpath(zArgv0,zPhlBinaryPath) != 0 ){` |
|  3456 |  159 | `		return zPhlBinaryPath;` |
|     - |  160 | `	}` |
|     - |  161 | `#endif` |
|   ! 0 |  162 | `	return zArgv0;` |
|  1733 |  163 | `}` |
|     - |  164 | `/*` |
|     - |  165 | ` * VM output consumer callback.` |
|     - |  166 | ` * Each time the virtual machine generates some outputs,the following` |
|     - |  167 | ` * function gets called by the underlying virtual machine to consume` |
|     - |  168 | ` * the generated output.` |
|     - |  169 | ` * All this function does is redirecting the VM output to STDOUT.` |
|     - |  170 | ` * This function is registered later via a call to ph7_vm_config()` |
|     - |  171 | ` * with a configuration verb set to: PH7_VM_CONFIG_OUTPUT.` |
|     - |  172 | ` */` |
| 11626 |  173 | `static int Output_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)` |
|     5 |  174 | `{` |
|  5813 |  175 | `	(void)pUserData;` |
|     - |  176 | `#ifdef __WINNT__` |
|     - |  177 | `	BOOL rc;` |
|     5 |  178 | `	rc = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),pOutput,(DWORD)nOutputLen,0,0);` |
|     5 |  179 | `	if( !rc ){` |
|     - |  180 | `		/* Abort processing */` |
|   ! 0 |  181 | `		return PH7_ABORT;` |
|     - |  182 | `	}` |
|     - |  183 | `#else` |
|     - |  184 | `	ssize_t nWr;` |
| 11626 |  185 | `	nWr = write(STDOUT_FILENO,pOutput,nOutputLen);` |
| 11626 |  186 | `	if( nWr < 0 ){` |
|     - |  187 | `		/* Abort processing */` |
|   ! 0 |  188 | `		return PH7_ABORT;` |
|     - |  189 | `	}` |
|     - |  190 | `#endif /* __WINT__ */` |
|     - |  191 | `	/* All done,VM output was redirected to STDOUT */` |
| 11631 |  192 | `	return PH7_OK;` |
|  5818 |  193 | `}` |
|     - |  194 | `/*` |
|     - |  195 | ` * Parse an unsigned-long testing knob from the environment (PHL_MAX_ALLOC /` |
|     - |  196 | ` * PHL_MAX_INPUT / PHL_MAX_RECURSION / PHL_MAX_NATIVE_DEPTH). Returns 1 and writes` |
|     - |  197 | ` * *pOut on a valid, strictly-positive, fully-numeric value clamped to` |
|     - |  198 | ` * [uFloor, uCeil]; returns` |
|     - |  199 | ` * 0 (leaving *pOut untouched) when the var is unset, empty, non-numeric, has` |
|     - |  200 | ` * trailing garbage, or is zero — so a typo like "-1" or "abc" is ignored` |
|     - |  201 | ` * rather than silently reinterpreted (strtoul would wrap "-1" to ULONG_MAX).` |
|     - |  202 | ` */` |
| 14488 |  203 | `static int PHL_EnvULong(const char *zName,unsigned long uFloor,unsigned long uCeil,unsigned long *pOut)` |
|     5 |  204 | `{` |
| 14493 |  205 | `	const char *zVal = getenv(zName);` |
| 14493 |  206 | `	char *zEnd = 0;` |
|     - |  207 | `	unsigned long uMax;` |
| 14493 |  208 | `	if( zVal == 0 \|\| zVal[0] == 0 ){` |
| 14479 |  209 | `		return 0;` |
|     - |  210 | `	}` |
|     - |  211 | `	/* Reject a leading sign outright: strtoul silently negates "-1" to` |
|     - |  212 | `	 * ULONG_MAX, turning a typo into an effectively-unlimited cap. */` |
|    17 |  213 | `	if( zVal[0] == '-' \|\| zVal[0] == '+' ){` |
|   ! 0 |  214 | `		return 0;` |
|     - |  215 | `	}` |
|    17 |  216 | `	errno = 0;` |
|    17 |  217 | `	uMax = strtoul(zVal,&zEnd,10);` |
|    17 |  218 | `	if( errno != 0 \|\| zEnd == zVal \|\| *zEnd != 0 \|\| uMax == 0 ){` |
|   ! 0 |  219 | `		return 0; /* non-numeric, trailing junk, overflow, or zero */` |
|     - |  220 | `	}` |
|    17 |  221 | `	if( uMax < uFloor ){` |
|   ! 0 |  222 | `		uMax = uFloor;` |
|   ! 0 |  223 | `	}` |
|    17 |  224 | `	if( uMax > uCeil ){` |
|   ! 0 |  225 | `		uMax = uCeil;` |
|   ! 0 |  226 | `	}` |
|    17 |  227 | `	*pOut = uMax;` |
|    17 |  228 | `	return 1;` |
|  7249 |  229 | `}` |
|     - |  230 | `/*` |
|     - |  231 | ` * Main program: Compile and execute the PHP file.` |
|     - |  232 | ` */` |
|  3995 |  233 | `int main(int argc,char **argv)` |
|     5 |  234 | `{` |
|     - |  235 | `	ph7 *pEngine; /* PH7 engine */` |
|     - |  236 | `	ph7_vm *pVm;  /* Compiled PHP program */` |
|  4000 |  237 | `	int dump_vm = 0;    /* Dump VM instructions if TRUE */` |
|  4000 |  238 | `	int run_code = 0;    /* Run inline code if TRUE */` |
|  4000 |  239 | `	int lint_mode = 0;   /* Syntax-check only (-l) if TRUE */` |
|  4000 |  240 | `	const char *zRunCode = 0; /* Inline code string */` |
|     - |  241 | `#ifdef PHL_ENABLE_SERVER` |
|  4000 |  242 | `	int server_mode = 0;        /* Start built-in server if TRUE */` |
|  4000 |  243 | `	const char *zServerAddr = 0; /* host:port string */` |
|  4000 |  244 | `	const char *zDocRoot = ".";  /* Document root */` |
|     - |  245 | `#endif` |
|     - |  246 | `	int n;              /* Script arguments */` |
|     - |  247 | `	int rc;` |
|     - |  248 | `	/* Process interpreter arguments first*/` |
|  4063 |  249 | `	for(n = 1 ; n < argc ; ++n ){` |
|     - |  250 | `		int c;` |
|  3877 |  251 | `		if( argv[n][0] != '-' ){` |
|     - |  252 | `			/* No more interpreter arguments */` |
|  3809 |  253 | `			break;` |
|     - |  254 | `		}` |
|     - |  255 | `		/* Check for long options */` |
|    72 |  256 | `		if( argv[n][1] == '-' ){` |
|    10 |  257 | `			if( strcmp(argv[n], "--version") == 0 ){` |
|     7 |  258 | `				Version();` |
|     6 |  259 | `			}else if( strcmp(argv[n], "--help") == 0 ){` |
|     3 |  260 | `				Help();` |
|     1 |  261 | `			}else{` |
|     - |  262 | `				/* Unknown long option */` |
|   ! 0 |  263 | `				Help();` |
|     - |  264 | `			}` |
|     4 |  265 | `			continue;` |
|     - |  266 | `		}` |
|    62 |  267 | `		c = argv[n][1];` |
|    62 |  268 | `		if( c == 'b' ){` |
|     - |  269 | `			/* Dump byte-code instructions */` |
|     3 |  270 | `			dump_vm = 1;` |
|    61 |  271 | `		}else if( c == 'l' ){` |
|     - |  272 | `			/* Syntax-check only (lint) the file argument that follows */` |
|     4 |  273 | `			lint_mode = 1;` |
|    58 |  274 | `		}else if( c == 'i' ){` |
|     - |  275 | `			/* Display interpreter information and exit */` |
|     2 |  276 | `			Info();` |
|    55 |  277 | `		}else if( c == 'r' ){` |
|     - |  278 | `			/* Run inline PHP code from next argument (php -r style) */` |
|    10 |  279 | `			if( n + 1 >= argc ){` |
|     - |  280 | `				/* Missing code argument */` |
|   ! 0 |  281 | `				FatalCode("Missing code argument for -r",1);` |
|   ! 0 |  282 | `			}` |
|    10 |  283 | `			zRunCode = argv[++n];` |
|    10 |  284 | `			run_code = 1;` |
|    48 |  285 | `		}else if( c == 'S' ){` |
|     - |  286 | `			/* Start built-in development server */` |
|     - |  287 | `#ifdef PHL_ENABLE_SERVER` |
|    22 |  288 | `			if( n + 1 >= argc ){` |
|   ! 0 |  289 | `				FatalCode("Missing host:port argument for -S",1);` |
|   ! 0 |  290 | `			}` |
|    22 |  291 | `			zServerAddr = argv[++n];` |
|    22 |  292 | `			server_mode = 1;` |
|     - |  293 | `#else` |
|     - |  294 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|     - |  295 | `#endif` |
|    33 |  296 | `		}else if( c == 't' ){` |
|     - |  297 | `			/* Set document root for the server */` |
|     - |  298 | `#ifdef PHL_ENABLE_SERVER` |
|    22 |  299 | `			if( n + 1 >= argc ){` |
|   ! 0 |  300 | `				FatalCode("Missing docroot argument for -t",1);` |
|   ! 0 |  301 | `			}` |
|    22 |  302 | `			zDocRoot = argv[++n];` |
|     - |  303 | `#else` |
|     - |  304 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|     - |  305 | `#endif` |
|    11 |  306 | `		}else if( c == 'v' ){` |
|     - |  307 | `			/* Display version */` |
|   ! 0 |  308 | `			Version();` |
|   ! 0 |  309 | `		}else{` |
|     - |  310 | `			/* Display a help message and exit */` |
|   ! 0 |  311 | `			Help();` |
|     - |  312 | `		}` |
|    32 |  313 | `	}` |
|     - |  314 | `#ifdef PHL_ENABLE_SERVER` |
|  3995 |  315 | `	if( server_mode ){` |
|     - |  316 | `		/* Parse host:port from zServerAddr */` |
|     - |  317 | `		char zHost[256];` |
|    22 |  318 | `		int iPort = 0;` |
|     - |  319 | `		const char *zColon;` |
|    22 |  320 | `		const char *zRouter = 0;` |
|    22 |  321 | `		zColon = strrchr(zServerAddr, ':');` |
|    22 |  322 | `		if( zColon == 0 ){` |
|   ! 0 |  323 | `			FatalCode("Invalid address format. Use host:port (e.g., localhost:8080)",1);` |
|   ! 0 |  324 | `		}` |
|     - |  325 | `		{` |
|    22 |  326 | `			int nHostLen = (int)(zColon - zServerAddr);` |
|    22 |  327 | `			if( nHostLen >= (int)sizeof(zHost) ) nHostLen = (int)sizeof(zHost) - 1;` |
|    22 |  328 | `			memcpy(zHost, zServerAddr, nHostLen);` |
|    22 |  329 | `			zHost[nHostLen] = 0;` |
|     - |  330 | `		}` |
|    22 |  331 | `		iPort = atoi(zColon + 1);` |
|    22 |  332 | `		if( iPort <= 0 \|\| iPort > 65535 ){` |
|   ! 0 |  333 | `			FatalCode("Invalid port number",1);` |
|   ! 0 |  334 | `		}` |
|     - |  335 | `		/* Check for optional router script */` |
|    22 |  336 | `		if( n < argc ){` |
|   ! 0 |  337 | `			zRouter = argv[n];` |
|   ! 0 |  338 | `		}` |
|    22 |  339 | `		return phl_serve(zHost, iPort, zDocRoot, zRouter, PHL_ResolveBinaryPath(argv[0]));` |
|     - |  340 | `	}` |
|     - |  341 | `#endif` |
|  3629 |  342 | `	if( n >= argc && !run_code ){` |
|   ! 0 |  343 | `		puts("Missing PHP file to compile");` |
|   ! 0 |  344 | `		Help();` |
|   ! 0 |  345 | `	}` |
|     - |  346 |  |
|     - |  347 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|     - |  348 | `	/* Install an unhandled exception minidump handler for Windows debug builds */` |
|     5 |  349 | `	CreateMiniDumpOnUnHandledException();` |
|     - |  350 | `#endif` |
|     - |  351 | `	/* Allocate a new PH7 engine instance */` |
|  3629 |  352 | `	rc = ph7_init(&pEngine);` |
|  3629 |  353 | `	if( rc != PH7_OK ){` |
|     - |  354 | `		/*` |
|     - |  355 | `		 * If the supplied memory subsystem is so sick that we are unable` |
|     - |  356 | `		 * to allocate a tiny chunk of memory,there is no much we can do here.` |
|     - |  357 | `		 */` |
|   ! 0 |  358 | `		Fatal("Error while allocating a new PH7 engine instance");` |
|   ! 0 |  359 | `	}` |
|     - |  360 | `	/* Set an error log consumer callback. This callback [Output_Consumer()] will` |
|     - |  361 | `	 * redirect all compile-time error messages to STDOUT.` |
|     - |  362 | `	 */` |
|  3629 |  363 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,` |
|     - |  364 | `		Output_Consumer, /* Error log consumer */` |
|     - |  365 |  |
|     - |  366 | `		);` |
|     - |  367 | `	/* Optional per-allocation memory cap (PHL_MAX_ALLOC=bytes). Used to` |
|     - |  368 | `	 * deterministically exercise out-of-memory paths (see tests/ph7/003-stress).` |
|     - |  369 | `	 * Clamp to a floor above the pool bucket size (SXMEM_POOL_MAXALLOC, 32 KB)` |
|     - |  370 | `	 * so the engine can still start; VMs inherit it at creation. */` |
|     - |  371 | `	{` |
|     - |  372 | `		unsigned long uMax;` |
|     - |  373 | `		/* floor: keep above the pool bucket size; clamp: nMaxRequest is 32-bit */` |
|  3629 |  374 | `		if( PHL_EnvULong("PHL_MAX_ALLOC",65536UL,0xFFFFFFFFUL,&uMax) ){` |
|   ! 0 |  375 | `			ph7_config(pEngine,PH7_CONFIG_MAX_ALLOC,(unsigned int)uMax);` |
|   ! 0 |  376 | `		}` |
|     - |  377 | `	}` |
|     - |  378 | `	/* Optional per-input byte cap (PHL_MAX_INPUT=bytes). Used to exercise the` |
|     - |  379 | `	 * input-size rejection path at a manageable scale (see tests/ph7/003-stress). */` |
|     - |  380 | `	{` |
|     - |  381 | `		unsigned long uMax;` |
|  3629 |  382 | `		if( PHL_EnvULong("PHL_MAX_INPUT",1UL,0xFFFFFFFFUL,&uMax) ){` |
|   ! 0 |  383 | `			ph7_config(pEngine,PH7_CONFIG_MAX_INPUT,(unsigned int)uMax);` |
|   ! 0 |  384 | `		}` |
|     - |  385 | `	}` |
|     - |  386 | `	/* Syntax-check only mode (-l): compile the target file, print PHP's summary` |
|     - |  387 | `	 * line and exit without executing. The error consumer installed above` |
|     - |  388 | `	 * already prints any parse error; ph7_compile_file leaves *pVm NULL on a` |
|     - |  389 | `	 * compile/IO error, so only a successful compile owns a VM to release. */` |
|  3629 |  390 | `	if( lint_mode ){` |
|     - |  391 | `		const char *zFile;` |
|     4 |  392 | `		if( n >= argc ){` |
|     - |  393 | ``			/* No file argument (e.g. `-l` alone, or `-l` mixed with `-r`). */`` |
|   ! 0 |  394 | `			ph7_release(pEngine);` |
|   ! 0 |  395 | `			puts("No input file specified");` |
|   ! 0 |  396 | `			return 255;` |
|     - |  397 | `		}` |
|     4 |  398 | `		zFile = argv[n];` |
|     4 |  399 | `		rc = ph7_compile_file(pEngine,zFile,&pVm,0);` |
|     4 |  400 | `		if( rc == PH7_OK ){` |
|     2 |  401 | `			printf("No syntax errors detected in %s\n",zFile);` |
|     2 |  402 | `			ph7_vm_release(pVm);` |
|     3 |  403 | `		}else if( rc == PH7_IO_ERR ){` |
|   ! 0 |  404 | `			printf("Could not open input file: %s\n",zFile);` |
|   ! 0 |  405 | `		}else{` |
|     2 |  406 | `			printf("Errors parsing %s\n",zFile);` |
|     - |  407 | `		}` |
|     4 |  408 | `		ph7_release(pEngine);` |
|     4 |  409 | `		return (rc == PH7_OK) ? 0 : 255;` |
|     - |  410 | `	}` |
|     - |  411 | `	/* Now,it's time to compile our PHP file */` |
|  3625 |  412 | `	if( run_code ){` |
|     - |  413 | `		/* Compile inline PHP code string (PHP only - no tags needed) */` |
|    10 |  414 | `		rc = ph7_compile_v2(` |
|     4 |  415 | `			pEngine, /* PH7 Engine */` |
|     4 |  416 | `			zRunCode, /* Source code */` |
|     - |  417 | `			-1,       /* Let API compute length */` |
|     - |  418 | `			&pVm,     /* OUT: Compiled PHP program */` |
|     - |  419 | `			PH7_PHP_ONLY /* Inline PHP, no tags expected */` |
|     - |  420 | `			);` |
|    10 |  421 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   ! 0 |  422 | `			if( rc == PH7_VM_ERR ){` |
|   ! 0 |  423 | `				Fatal("VM initialization error");` |
|   ! 0 |  424 | `			}else{` |
|     - |  425 | `				/* Compile-time error, your output (STDOUT) should display the error messages */` |
|   ! 0 |  426 | `				Fatal("Compile error");` |
|     - |  427 | `			}` |
|   ! 0 |  428 | `		}` |
|     6 |  429 | `	}else{` |
|  3617 |  430 | `		rc = ph7_compile_file(` |
|  1713 |  431 | `			pEngine, /* PH7 Engine */` |
|  3612 |  432 | `			argv[n], /* Path to the PHP file to compile */` |
|     - |  433 | `			&pVm,    /* OUT: Compiled PHP program */` |
|     - |  434 |  |
|     - |  435 | `			);` |
|  3617 |  436 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   376 |  437 | `			if( rc == PH7_IO_ERR ){` |
|   ! 0 |  438 | `				FatalCode("IO error while opening the target file",1);` |
|   376 |  439 | `			}else if( rc == PH7_VM_ERR ){` |
|   ! 0 |  440 | `				Fatal("VM initialization error");` |
|   ! 0 |  441 | `			}else{` |
|     - |  442 | `				/* Compile-time error, your output (STDOUT) should display the error messages */` |
|   376 |  443 | `				Fatal("Compile error");` |
|     - |  444 | `			}` |
|   186 |  445 | `		}` |
|     - |  446 | `	}` |
|     - |  447 | `	/*` |
|     - |  448 | `	 * Now we have our script compiled,it's time to configure our VM.` |
|     - |  449 | `	 * We will install the VM output consumer callback defined above` |
|     - |  450 | `	 * so that we can consume the VM output and redirect it to STDOUT.` |
|     - |  451 | `	 */` |
|  3439 |  452 | `	rc = ph7_vm_config(pVm,` |
|     - |  453 | `		PH7_VM_CONFIG_OUTPUT,` |
|     - |  454 | `		Output_Consumer,    /* Output Consumer callback */` |
|     - |  455 |  |
|     - |  456 | `		);` |
|  3439 |  457 | `	if( rc != PH7_OK ){` |
|   ! 0 |  458 | `		Fatal("Error while installing the VM output consumer callback");` |
|   ! 0 |  459 | `	}` |
|     - |  460 | `	/* Optional recursion caps via the environment (like PHL_MAX_ALLOC). The host` |
|     - |  461 | `	 * defaults are PHP-parity — PHP call depth is UNBOUNDED (heap-bound) and only` |
|     - |  462 | `	 * the native VmByteCodeExec nesting is capped — so these knobs are for tests` |
|     - |  463 | `	 * and embedders that want a tighter bound, not to raise a low default.` |
|     - |  464 | `	 *   PHL_MAX_RECURSION   -> PH7_VM_CONFIG_RECURSION_DEPTH (PHP call depth; any` |
|     - |  465 | `	 *                          positive value is a cap, PHL_EnvULong rejects 0)` |
|     - |  466 | `	 *   PHL_MAX_NATIVE_DEPTH -> PH7_VM_CONFIG_NATIVE_DEPTH   (native nesting;` |
|     - |  467 | `	 *                          floor 2) */` |
|     - |  468 | `	{` |
|     - |  469 | `		unsigned long uMax;` |
|  3439 |  470 | `		if( PHL_EnvULong("PHL_MAX_RECURSION",1UL,0x7FFFFFFFUL,&uMax) ){` |
|     5 |  471 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_RECURSION_DEPTH,(int)uMax);` |
|     2 |  472 | `		}` |
|  3439 |  473 | `		if( PHL_EnvULong("PHL_MAX_NATIVE_DEPTH",2UL,0x7FFFFFFFUL,&uMax) ){` |
|    12 |  474 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_NATIVE_DEPTH,(int)uMax);` |
|     5 |  475 | `		}` |
|     - |  476 | `	}` |
|     - |  477 | `	/* Define PHP_BINARY: absolute path of this interpreter */` |
|  5156 |  478 | `	ph7_create_constant(pVm,"PHP_BINARY",PHL_PhpBinaryConst,` |
|  3434 |  479 | `		(void *)PHL_ResolveBinaryPath(argv[0]));` |
|     - |  480 | `	/* Register the script arguments as $argv[] plus the matching $argc count and` |
|     - |  481 | `	 * the CLI $_SERVER entries, matching PHP: $argv[0] is the script path (file` |
|     - |  482 | `	 * mode) or the literal "Standard input code" (-r mode), followed by the` |
|     - |  483 | `	 * script's own arguments.` |
|     - |  484 | `	 */` |
|     - |  485 | `	{` |
|  3439 |  486 | `		const char *zScriptName = run_code ? "Standard input code" : argv[n];` |
|  3439 |  487 | `		int argv_count = 0;` |
|     - |  488 | `		ph7_value *pArgc;` |
|     - |  489 | `		/* Count only the entries actually inserted: PH7_VM_CONFIG_ARGV_ENTRY skips` |
|     - |  490 | `		 * an empty string, so counting unconditionally would leave $argc greater` |
|     - |  491 | ``		 * than count($argv) for an empty argument (e.g. `phl s.php "" x`). */`` |
|  3439 |  492 | `		if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,zScriptName) == PH7_OK ){` |
|  3439 |  493 | `			argv_count++;` |
|  1717 |  494 | `		}` |
|     - |  495 | `		/* The script's own arguments follow: in file mode argv[n] is the script` |
|     - |  496 | `		 * (registered above), so they start at n+1; in -r mode they start at n. */` |
|  3469 |  497 | `		for( n = run_code ? n : n + 1; n < argc ; ++n ){` |
|    35 |  498 | `			if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,argv[n]) == PH7_OK ){` |
|    33 |  499 | `				argv_count++;` |
|    14 |  500 | `			}` |
|    20 |  501 | `		}` |
|     - |  502 | `		/* $argc: a plain integer global equal to count($argv). */` |
|  3439 |  503 | `		pArgc = ph7_new_scalar(pVm);` |
|  3439 |  504 | `		if( pArgc ){` |
|  3439 |  505 | `			ph7_value_int(pArgc,argv_count);` |
|  3439 |  506 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_CREATE_VAR,"argc",pArgc);` |
|  3439 |  507 | `			ph7_release_value(pVm,pArgc);` |
|  1717 |  508 | `		}` |
|     - |  509 | `		/* $_SERVER entries frameworks read at CLI bootstrap. SCRIPT_FILENAME is` |
|     - |  510 | `		 * already set to the script path by PH7_HashmapCreateSuper. */` |
|  3439 |  511 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"SCRIPT_NAME",zScriptName,-1);` |
|  3439 |  512 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PHP_SELF",zScriptName,-1);` |
|  3439 |  513 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"DOCUMENT_ROOT","",0);` |
|     - |  514 | `		{` |
|     - |  515 | `			char zTime[32];` |
|  3439 |  516 | `			snprintf(zTime,sizeof(zTime),"%ld",(long)time(0));` |
|  3439 |  517 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"REQUEST_TIME",zTime,-1);` |
|     - |  518 | `		}` |
|     - |  519 | `#ifndef __WINNT__` |
|     - |  520 | `		{` |
|     - |  521 | `			char zCwd[PATH_MAX];` |
|  3434 |  522 | `			if( getcwd(zCwd,sizeof(zCwd)) ){` |
|  3434 |  523 | `				ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PWD",zCwd,-1);` |
|  1717 |  524 | `			}` |
|     - |  525 | `		}` |
|     - |  526 | `#endif` |
|     - |  527 | `	}` |
|     - |  528 | `	/* Report script run-time errors (now default behavior) */` |
|  3439 |  529 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_ERR_REPORT);` |
|  3439 |  530 | `	if( dump_vm ){` |
|     - |  531 | `		/* Dump PH7 byte-code instructions */` |
|     3 |  532 | `		ph7_vm_dump_v2(pVm,` |
|     - |  533 | `			Output_Consumer, /* Dump consumer callback */` |
|     - |  534 |  |
|     - |  535 | `			);` |
|     1 |  536 | `	}` |
|     - |  537 | `	/*` |
|     - |  538 | `	 * And finally, execute our program. Note that your output (STDOUT in our case)` |
|     - |  539 | `	 * should display the result.` |
|     - |  540 | `	 */` |
|     - |  541 | `	{` |
|  3439 |  542 | `		int iExitStatus = 0;` |
|  3439 |  543 | `		ph7_vm_exec(pVm,&iExitStatus);` |
|     - |  544 | `		/* All done, cleanup the mess left behind.` |
|     - |  545 | `		*/` |
|  3439 |  546 | `		ph7_vm_release(pVm);` |
|  3439 |  547 | `		ph7_release(pEngine);` |
|     - |  548 | `		/* Propagate the script exit status (set via exit()/die()) */` |
|  3439 |  549 | `		return iExitStatus;` |
|     - |  550 | `	}` |
|  1735 |  551 | `}` |
|     - |  552 |  |
