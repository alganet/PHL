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
|  3446 |  150 | `static const char * PHL_ResolveBinaryPath(const char *zArgv0)` |
|     5 |  151 | `{` |
|     - |  152 | `#ifdef __WINNT__` |
|     5 |  153 | `	DWORD nLen = GetModuleFileNameA(0,zPhlBinaryPath,(DWORD)sizeof(zPhlBinaryPath));` |
|     5 |  154 | `	if( nLen > 0 && nLen < sizeof(zPhlBinaryPath) ){` |
|     5 |  155 | `		return zPhlBinaryPath;` |
|     - |  156 | `	}` |
|     - |  157 | `#else` |
|  3446 |  158 | `	if( realpath(zArgv0,zPhlBinaryPath) != 0 ){` |
|  3446 |  159 | `		return zPhlBinaryPath;` |
|     - |  160 | `	}` |
|     - |  161 | `#endif` |
|   ! 0 |  162 | `	return zArgv0;` |
|  1728 |  163 | `}` |
|     - |  164 | `/*` |
|     - |  165 | ` * VM output consumer callback.` |
|     - |  166 | ` * Each time the virtual machine generates some outputs,the following` |
|     - |  167 | ` * function gets called by the underlying virtual machine to consume` |
|     - |  168 | ` * the generated output.` |
|     - |  169 | ` * All this function does is redirecting the VM output to STDOUT.` |
|     - |  170 | ` * This function is registered later via a call to ph7_vm_config()` |
|     - |  171 | ` * with a configuration verb set to: PH7_VM_CONFIG_OUTPUT.` |
|     - |  172 | ` */` |
| 11348 |  173 | `static int Output_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)` |
|     5 |  174 | `{` |
|  5674 |  175 | `	(void)pUserData;` |
|     - |  176 | `#ifdef __WINNT__` |
|     - |  177 | `	BOOL rc;` |
|     5 |  178 | `	rc = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),pOutput,(DWORD)nOutputLen,0,0);` |
|     5 |  179 | `	if( !rc ){` |
|     - |  180 | `		/* Abort processing */` |
|   ! 0 |  181 | `		return PH7_ABORT;` |
|     - |  182 | `	}` |
|     - |  183 | `#else` |
|     - |  184 | `	ssize_t nWr;` |
| 11348 |  185 | `	nWr = write(STDOUT_FILENO,pOutput,nOutputLen);` |
| 11348 |  186 | `	if( nWr < 0 ){` |
|     - |  187 | `		/* Abort processing */` |
|   ! 0 |  188 | `		return PH7_ABORT;` |
|     - |  189 | `	}` |
|     - |  190 | `#endif /* __WINT__ */` |
|     - |  191 | `	/* All done,VM output was redirected to STDOUT */` |
| 11353 |  192 | `	return PH7_OK;` |
|  5679 |  193 | `}` |
|     - |  194 | `/*` |
|     - |  195 | ` * Parse an unsigned-long testing knob from the environment (PHL_MAX_ALLOC /` |
|     - |  196 | ` * PHL_MAX_INPUT / PHL_MAX_RECURSION / PHL_MAX_NATIVE_DEPTH). Returns 1 and writes *pOut on a valid,` |
|     - |  197 | ` * strictly-positive, fully-numeric value clamped to [uFloor, uCeil]; returns` |
|     - |  198 | ` * 0 (leaving *pOut untouched) when the var is unset, empty, non-numeric, has` |
|     - |  199 | ` * trailing garbage, or is zero — so a typo like "-1" or "abc" is ignored` |
|     - |  200 | ` * rather than silently reinterpreted (strtoul would wrap "-1" to ULONG_MAX).` |
|     - |  201 | ` */` |
| 14448 |  202 | `static int PHL_EnvULong(const char *zName,unsigned long uFloor,unsigned long uCeil,unsigned long *pOut)` |
|     5 |  203 | `{` |
| 14453 |  204 | `	const char *zVal = getenv(zName);` |
| 14453 |  205 | `	char *zEnd = 0;` |
|     - |  206 | `	unsigned long uMax;` |
| 14453 |  207 | `	if( zVal == 0 \|\| zVal[0] == 0 ){` |
| 14439 |  208 | `		return 0;` |
|     - |  209 | `	}` |
|     - |  210 | `	/* Reject a leading sign outright: strtoul silently negates "-1" to` |
|     - |  211 | `	 * ULONG_MAX, turning a typo into an effectively-unlimited cap. */` |
|    16 |  212 | `	if( zVal[0] == '-' \|\| zVal[0] == '+' ){` |
|   ! 0 |  213 | `		return 0;` |
|     - |  214 | `	}` |
|    16 |  215 | `	errno = 0;` |
|    16 |  216 | `	uMax = strtoul(zVal,&zEnd,10);` |
|    16 |  217 | `	if( errno != 0 \|\| zEnd == zVal \|\| *zEnd != 0 \|\| uMax == 0 ){` |
|   ! 0 |  218 | `		return 0; /* non-numeric, trailing junk, overflow, or zero */` |
|     - |  219 | `	}` |
|    16 |  220 | `	if( uMax < uFloor ){` |
|   ! 0 |  221 | `		uMax = uFloor;` |
|   ! 0 |  222 | `	}` |
|    16 |  223 | `	if( uMax > uCeil ){` |
|   ! 0 |  224 | `		uMax = uCeil;` |
|   ! 0 |  225 | `	}` |
|    16 |  226 | `	*pOut = uMax;` |
|    16 |  227 | `	return 1;` |
|  7229 |  228 | `}` |
|     - |  229 | `/*` |
|     - |  230 | ` * Main program: Compile and execute the PHP file.` |
|     - |  231 | ` */` |
|  3985 |  232 | `int main(int argc,char **argv)` |
|     5 |  233 | `{` |
|     - |  234 | `	ph7 *pEngine; /* PH7 engine */` |
|     - |  235 | `	ph7_vm *pVm;  /* Compiled PHP program */` |
|  3990 |  236 | `	int dump_vm = 0;    /* Dump VM instructions if TRUE */` |
|  3990 |  237 | `	int run_code = 0;    /* Run inline code if TRUE */` |
|  3990 |  238 | `	int lint_mode = 0;   /* Syntax-check only (-l) if TRUE */` |
|  3990 |  239 | `	const char *zRunCode = 0; /* Inline code string */` |
|     - |  240 | `#ifdef PHL_ENABLE_SERVER` |
|  3990 |  241 | `	int server_mode = 0;        /* Start built-in server if TRUE */` |
|  3990 |  242 | `	const char *zServerAddr = 0; /* host:port string */` |
|  3990 |  243 | `	const char *zDocRoot = ".";  /* Document root */` |
|     - |  244 | `#endif` |
|     - |  245 | `	int n;              /* Script arguments */` |
|     - |  246 | `	int rc;` |
|     - |  247 | `	/* Process interpreter arguments first*/` |
|  4053 |  248 | `	for(n = 1 ; n < argc ; ++n ){` |
|     - |  249 | `		int c;` |
|  3867 |  250 | `		if( argv[n][0] != '-' ){` |
|     - |  251 | `			/* No more interpreter arguments */` |
|  3799 |  252 | `			break;` |
|     - |  253 | `		}` |
|     - |  254 | `		/* Check for long options */` |
|    72 |  255 | `		if( argv[n][1] == '-' ){` |
|    10 |  256 | `			if( strcmp(argv[n], "--version") == 0 ){` |
|     7 |  257 | `				Version();` |
|     6 |  258 | `			}else if( strcmp(argv[n], "--help") == 0 ){` |
|     3 |  259 | `				Help();` |
|     1 |  260 | `			}else{` |
|     - |  261 | `				/* Unknown long option */` |
|   ! 0 |  262 | `				Help();` |
|     - |  263 | `			}` |
|     4 |  264 | `			continue;` |
|     - |  265 | `		}` |
|    62 |  266 | `		c = argv[n][1];` |
|    62 |  267 | `		if( c == 'b' ){` |
|     - |  268 | `			/* Dump byte-code instructions */` |
|     3 |  269 | `			dump_vm = 1;` |
|    61 |  270 | `		}else if( c == 'l' ){` |
|     - |  271 | `			/* Syntax-check only (lint) the file argument that follows */` |
|     4 |  272 | `			lint_mode = 1;` |
|    58 |  273 | `		}else if( c == 'i' ){` |
|     - |  274 | `			/* Display interpreter information and exit */` |
|     2 |  275 | `			Info();` |
|    55 |  276 | `		}else if( c == 'r' ){` |
|     - |  277 | `			/* Run inline PHP code from next argument (php -r style) */` |
|    10 |  278 | `			if( n + 1 >= argc ){` |
|     - |  279 | `				/* Missing code argument */` |
|   ! 0 |  280 | `				FatalCode("Missing code argument for -r",1);` |
|   ! 0 |  281 | `			}` |
|    10 |  282 | `			zRunCode = argv[++n];` |
|    10 |  283 | `			run_code = 1;` |
|    48 |  284 | `		}else if( c == 'S' ){` |
|     - |  285 | `			/* Start built-in development server */` |
|     - |  286 | `#ifdef PHL_ENABLE_SERVER` |
|    22 |  287 | `			if( n + 1 >= argc ){` |
|   ! 0 |  288 | `				FatalCode("Missing host:port argument for -S",1);` |
|   ! 0 |  289 | `			}` |
|    22 |  290 | `			zServerAddr = argv[++n];` |
|    22 |  291 | `			server_mode = 1;` |
|     - |  292 | `#else` |
|     - |  293 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|     - |  294 | `#endif` |
|    33 |  295 | `		}else if( c == 't' ){` |
|     - |  296 | `			/* Set document root for the server */` |
|     - |  297 | `#ifdef PHL_ENABLE_SERVER` |
|    22 |  298 | `			if( n + 1 >= argc ){` |
|   ! 0 |  299 | `				FatalCode("Missing docroot argument for -t",1);` |
|   ! 0 |  300 | `			}` |
|    22 |  301 | `			zDocRoot = argv[++n];` |
|     - |  302 | `#else` |
|     - |  303 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|     - |  304 | `#endif` |
|    11 |  305 | `		}else if( c == 'v' ){` |
|     - |  306 | `			/* Display version */` |
|   ! 0 |  307 | `			Version();` |
|   ! 0 |  308 | `		}else{` |
|     - |  309 | `			/* Display a help message and exit */` |
|   ! 0 |  310 | `			Help();` |
|     - |  311 | `		}` |
|    32 |  312 | `	}` |
|     - |  313 | `#ifdef PHL_ENABLE_SERVER` |
|  3985 |  314 | `	if( server_mode ){` |
|     - |  315 | `		/* Parse host:port from zServerAddr */` |
|     - |  316 | `		char zHost[256];` |
|    22 |  317 | `		int iPort = 0;` |
|     - |  318 | `		const char *zColon;` |
|    22 |  319 | `		const char *zRouter = 0;` |
|    22 |  320 | `		zColon = strrchr(zServerAddr, ':');` |
|    22 |  321 | `		if( zColon == 0 ){` |
|   ! 0 |  322 | `			FatalCode("Invalid address format. Use host:port (e.g., localhost:8080)",1);` |
|   ! 0 |  323 | `		}` |
|     - |  324 | `		{` |
|    22 |  325 | `			int nHostLen = (int)(zColon - zServerAddr);` |
|    22 |  326 | `			if( nHostLen >= (int)sizeof(zHost) ) nHostLen = (int)sizeof(zHost) - 1;` |
|    22 |  327 | `			memcpy(zHost, zServerAddr, nHostLen);` |
|    22 |  328 | `			zHost[nHostLen] = 0;` |
|     - |  329 | `		}` |
|    22 |  330 | `		iPort = atoi(zColon + 1);` |
|    22 |  331 | `		if( iPort <= 0 \|\| iPort > 65535 ){` |
|   ! 0 |  332 | `			FatalCode("Invalid port number",1);` |
|   ! 0 |  333 | `		}` |
|     - |  334 | `		/* Check for optional router script */` |
|    22 |  335 | `		if( n < argc ){` |
|   ! 0 |  336 | `			zRouter = argv[n];` |
|   ! 0 |  337 | `		}` |
|    22 |  338 | `		return phl_serve(zHost, iPort, zDocRoot, zRouter, PHL_ResolveBinaryPath(argv[0]));` |
|     - |  339 | `	}` |
|     - |  340 | `#endif` |
|  3619 |  341 | `	if( n >= argc && !run_code ){` |
|   ! 0 |  342 | `		puts("Missing PHP file to compile");` |
|   ! 0 |  343 | `		Help();` |
|   ! 0 |  344 | `	}` |
|     - |  345 |  |
|     - |  346 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|     - |  347 | `	/* Install an unhandled exception minidump handler for Windows debug builds */` |
|     5 |  348 | `	CreateMiniDumpOnUnHandledException();` |
|     - |  349 | `#endif` |
|     - |  350 | `	/* Allocate a new PH7 engine instance */` |
|  3619 |  351 | `	rc = ph7_init(&pEngine);` |
|  3619 |  352 | `	if( rc != PH7_OK ){` |
|     - |  353 | `		/*` |
|     - |  354 | `		 * If the supplied memory subsystem is so sick that we are unable` |
|     - |  355 | `		 * to allocate a tiny chunk of memory,there is no much we can do here.` |
|     - |  356 | `		 */` |
|   ! 0 |  357 | `		Fatal("Error while allocating a new PH7 engine instance");` |
|   ! 0 |  358 | `	}` |
|     - |  359 | `	/* Set an error log consumer callback. This callback [Output_Consumer()] will` |
|     - |  360 | `	 * redirect all compile-time error messages to STDOUT.` |
|     - |  361 | `	 */` |
|  3619 |  362 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,` |
|     - |  363 | `		Output_Consumer, /* Error log consumer */` |
|     - |  364 |  |
|     - |  365 | `		);` |
|     - |  366 | `	/* Optional per-allocation memory cap (PHL_MAX_ALLOC=bytes). Used to` |
|     - |  367 | `	 * deterministically exercise out-of-memory paths (see tests/ph7/003-stress).` |
|     - |  368 | `	 * Clamp to a floor above the pool bucket size (SXMEM_POOL_MAXALLOC, 32 KB)` |
|     - |  369 | `	 * so the engine can still start; VMs inherit it at creation. */` |
|     - |  370 | `	{` |
|     - |  371 | `		unsigned long uMax;` |
|     - |  372 | `		/* floor: keep above the pool bucket size; clamp: nMaxRequest is 32-bit */` |
|  3619 |  373 | `		if( PHL_EnvULong("PHL_MAX_ALLOC",65536UL,0xFFFFFFFFUL,&uMax) ){` |
|   ! 0 |  374 | `			ph7_config(pEngine,PH7_CONFIG_MAX_ALLOC,(unsigned int)uMax);` |
|   ! 0 |  375 | `		}` |
|     - |  376 | `	}` |
|     - |  377 | `	/* Optional per-input byte cap (PHL_MAX_INPUT=bytes). Used to exercise the` |
|     - |  378 | `	 * input-size rejection path at a manageable scale (see tests/ph7/003-stress). */` |
|     - |  379 | `	{` |
|     - |  380 | `		unsigned long uMax;` |
|  3619 |  381 | `		if( PHL_EnvULong("PHL_MAX_INPUT",1UL,0xFFFFFFFFUL,&uMax) ){` |
|   ! 0 |  382 | `			ph7_config(pEngine,PH7_CONFIG_MAX_INPUT,(unsigned int)uMax);` |
|   ! 0 |  383 | `		}` |
|     - |  384 | `	}` |
|     - |  385 | `	/* Syntax-check only mode (-l): compile the target file, print PHP's summary` |
|     - |  386 | `	 * line and exit without executing. The error consumer installed above` |
|     - |  387 | `	 * already prints any parse error; ph7_compile_file leaves *pVm NULL on a` |
|     - |  388 | `	 * compile/IO error, so only a successful compile owns a VM to release. */` |
|  3619 |  389 | `	if( lint_mode ){` |
|     - |  390 | `		const char *zFile;` |
|     4 |  391 | `		if( n >= argc ){` |
|     - |  392 | ``			/* No file argument (e.g. `-l` alone, or `-l` mixed with `-r`). */`` |
|   ! 0 |  393 | `			ph7_release(pEngine);` |
|   ! 0 |  394 | `			puts("No input file specified");` |
|   ! 0 |  395 | `			return 255;` |
|     - |  396 | `		}` |
|     4 |  397 | `		zFile = argv[n];` |
|     4 |  398 | `		rc = ph7_compile_file(pEngine,zFile,&pVm,0);` |
|     4 |  399 | `		if( rc == PH7_OK ){` |
|     2 |  400 | `			printf("No syntax errors detected in %s\n",zFile);` |
|     2 |  401 | `			ph7_vm_release(pVm);` |
|     3 |  402 | `		}else if( rc == PH7_IO_ERR ){` |
|   ! 0 |  403 | `			printf("Could not open input file: %s\n",zFile);` |
|   ! 0 |  404 | `		}else{` |
|     2 |  405 | `			printf("Errors parsing %s\n",zFile);` |
|     - |  406 | `		}` |
|     4 |  407 | `		ph7_release(pEngine);` |
|     4 |  408 | `		return (rc == PH7_OK) ? 0 : 255;` |
|     - |  409 | `	}` |
|     - |  410 | `	/* Now,it's time to compile our PHP file */` |
|  3615 |  411 | `	if( run_code ){` |
|     - |  412 | `		/* Compile inline PHP code string (PHP only - no tags needed) */` |
|    10 |  413 | `		rc = ph7_compile_v2(` |
|     4 |  414 | `			pEngine, /* PH7 Engine */` |
|     4 |  415 | `			zRunCode, /* Source code */` |
|     - |  416 | `			-1,       /* Let API compute length */` |
|     - |  417 | `			&pVm,     /* OUT: Compiled PHP program */` |
|     - |  418 | `			PH7_PHP_ONLY /* Inline PHP, no tags expected */` |
|     - |  419 | `			);` |
|    10 |  420 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   ! 0 |  421 | `			if( rc == PH7_VM_ERR ){` |
|   ! 0 |  422 | `				Fatal("VM initialization error");` |
|   ! 0 |  423 | `			}else{` |
|     - |  424 | `				/* Compile-time error, your output (STDOUT) should display the error messages */` |
|   ! 0 |  425 | `				Fatal("Compile error");` |
|     - |  426 | `			}` |
|   ! 0 |  427 | `		}` |
|     6 |  428 | `	}else{` |
|  3607 |  429 | `		rc = ph7_compile_file(` |
|  1708 |  430 | `			pEngine, /* PH7 Engine */` |
|  3602 |  431 | `			argv[n], /* Path to the PHP file to compile */` |
|     - |  432 | `			&pVm,    /* OUT: Compiled PHP program */` |
|     - |  433 |  |
|     - |  434 | `			);` |
|  3607 |  435 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   376 |  436 | `			if( rc == PH7_IO_ERR ){` |
|   ! 0 |  437 | `				FatalCode("IO error while opening the target file",1);` |
|   376 |  438 | `			}else if( rc == PH7_VM_ERR ){` |
|   ! 0 |  439 | `				Fatal("VM initialization error");` |
|   ! 0 |  440 | `			}else{` |
|     - |  441 | `				/* Compile-time error, your output (STDOUT) should display the error messages */` |
|   376 |  442 | `				Fatal("Compile error");` |
|     - |  443 | `			}` |
|   186 |  444 | `		}` |
|     - |  445 | `	}` |
|     - |  446 | `	/*` |
|     - |  447 | `	 * Now we have our script compiled,it's time to configure our VM.` |
|     - |  448 | `	 * We will install the VM output consumer callback defined above` |
|     - |  449 | `	 * so that we can consume the VM output and redirect it to STDOUT.` |
|     - |  450 | `	 */` |
|  3429 |  451 | `	rc = ph7_vm_config(pVm,` |
|     - |  452 | `		PH7_VM_CONFIG_OUTPUT,` |
|     - |  453 | `		Output_Consumer,    /* Output Consumer callback */` |
|     - |  454 |  |
|     - |  455 | `		);` |
|  3429 |  456 | `	if( rc != PH7_OK ){` |
|   ! 0 |  457 | `		Fatal("Error while installing the VM output consumer callback");` |
|   ! 0 |  458 | `	}` |
|     - |  459 | `	/* Optional recursion caps via the environment (like PHL_MAX_ALLOC). The host` |
|     - |  460 | `	 * defaults are PHP-parity — PHP call depth is UNBOUNDED (heap-bound) and only` |
|     - |  461 | `	 * the native VmByteCodeExec nesting is capped — so these knobs are for tests` |
|     - |  462 | `	 * and embedders that want a tighter bound, not to raise a low default.` |
|     - |  463 | `	 *   PHL_MAX_RECURSION   -> PH7_VM_CONFIG_RECURSION_DEPTH (PHP call depth; any` |
|     - |  464 | `	 *                          positive value is a cap, PHL_EnvULong rejects 0)` |
|     - |  465 | `	 *   PHL_MAX_NATIVE_DEPTH -> PH7_VM_CONFIG_NATIVE_DEPTH   (native nesting;` |
|     - |  466 | `	 *                          floor 2) */` |
|     - |  467 | `	{` |
|     - |  468 | `		unsigned long uMax;` |
|  3429 |  469 | `		if( PHL_EnvULong("PHL_MAX_RECURSION",1UL,0x7FFFFFFFUL,&uMax) ){` |
|     5 |  470 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_RECURSION_DEPTH,(int)uMax);` |
|     2 |  471 | `		}` |
|  3429 |  472 | `		if( PHL_EnvULong("PHL_MAX_NATIVE_DEPTH",2UL,0x7FFFFFFFUL,&uMax) ){` |
|    12 |  473 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_NATIVE_DEPTH,(int)uMax);` |
|     5 |  474 | `		}` |
|     - |  475 | `	}` |
|     - |  476 | `	/* Define PHP_BINARY: absolute path of this interpreter */` |
|  5141 |  477 | `	ph7_create_constant(pVm,"PHP_BINARY",PHL_PhpBinaryConst,` |
|  3424 |  478 | `		(void *)PHL_ResolveBinaryPath(argv[0]));` |
|     - |  479 | `	/* Register the script arguments as $argv[] plus the matching $argc count and` |
|     - |  480 | `	 * the CLI $_SERVER entries, matching PHP: $argv[0] is the script path (file` |
|     - |  481 | `	 * mode) or the literal "Standard input code" (-r mode), followed by the` |
|     - |  482 | `	 * script's own arguments.` |
|     - |  483 | `	 */` |
|     - |  484 | `	{` |
|  3429 |  485 | `		const char *zScriptName = run_code ? "Standard input code" : argv[n];` |
|  3429 |  486 | `		int argv_count = 0;` |
|     - |  487 | `		ph7_value *pArgc;` |
|     - |  488 | `		/* Count only the entries actually inserted: PH7_VM_CONFIG_ARGV_ENTRY skips` |
|     - |  489 | `		 * an empty string, so counting unconditionally would leave $argc greater` |
|     - |  490 | ``		 * than count($argv) for an empty argument (e.g. `phl s.php "" x`). */`` |
|  3429 |  491 | `		if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,zScriptName) == PH7_OK ){` |
|  3429 |  492 | `			argv_count++;` |
|  1712 |  493 | `		}` |
|     - |  494 | `		/* The script's own arguments follow: in file mode argv[n] is the script` |
|     - |  495 | `		 * (registered above), so they start at n+1; in -r mode they start at n. */` |
|  3459 |  496 | `		for( n = run_code ? n : n + 1; n < argc ; ++n ){` |
|    35 |  497 | `			if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,argv[n]) == PH7_OK ){` |
|    33 |  498 | `				argv_count++;` |
|    14 |  499 | `			}` |
|    20 |  500 | `		}` |
|     - |  501 | `		/* $argc: a plain integer global equal to count($argv). */` |
|  3429 |  502 | `		pArgc = ph7_new_scalar(pVm);` |
|  3429 |  503 | `		if( pArgc ){` |
|  3429 |  504 | `			ph7_value_int(pArgc,argv_count);` |
|  3429 |  505 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_CREATE_VAR,"argc",pArgc);` |
|  3429 |  506 | `			ph7_release_value(pVm,pArgc);` |
|  1712 |  507 | `		}` |
|     - |  508 | `		/* $_SERVER entries frameworks read at CLI bootstrap. SCRIPT_FILENAME is` |
|     - |  509 | `		 * already set to the script path by PH7_HashmapCreateSuper. */` |
|  3429 |  510 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"SCRIPT_NAME",zScriptName,-1);` |
|  3429 |  511 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PHP_SELF",zScriptName,-1);` |
|  3429 |  512 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"DOCUMENT_ROOT","",0);` |
|     - |  513 | `		{` |
|     - |  514 | `			char zTime[32];` |
|  3429 |  515 | `			snprintf(zTime,sizeof(zTime),"%ld",(long)time(0));` |
|  3429 |  516 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"REQUEST_TIME",zTime,-1);` |
|     - |  517 | `		}` |
|     - |  518 | `#ifndef __WINNT__` |
|     - |  519 | `		{` |
|     - |  520 | `			char zCwd[PATH_MAX];` |
|  3424 |  521 | `			if( getcwd(zCwd,sizeof(zCwd)) ){` |
|  3424 |  522 | `				ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PWD",zCwd,-1);` |
|  1712 |  523 | `			}` |
|     - |  524 | `		}` |
|     - |  525 | `#endif` |
|     - |  526 | `	}` |
|     - |  527 | `	/* Report script run-time errors (now default behavior) */` |
|  3429 |  528 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_ERR_REPORT);` |
|  3429 |  529 | `	if( dump_vm ){` |
|     - |  530 | `		/* Dump PH7 byte-code instructions */` |
|     3 |  531 | `		ph7_vm_dump_v2(pVm,` |
|     - |  532 | `			Output_Consumer, /* Dump consumer callback */` |
|     - |  533 |  |
|     - |  534 | `			);` |
|     1 |  535 | `	}` |
|     - |  536 | `	/*` |
|     - |  537 | `	 * And finally, execute our program. Note that your output (STDOUT in our case)` |
|     - |  538 | `	 * should display the result.` |
|     - |  539 | `	 */` |
|     - |  540 | `	{` |
|  3429 |  541 | `		int iExitStatus = 0;` |
|  3429 |  542 | `		ph7_vm_exec(pVm,&iExitStatus);` |
|     - |  543 | `		/* All done, cleanup the mess left behind.` |
|     - |  544 | `		*/` |
|  3429 |  545 | `		ph7_vm_release(pVm);` |
|  3429 |  546 | `		ph7_release(pEngine);` |
|     - |  547 | `		/* Propagate the script exit status (set via exit()/die()) */` |
|  3429 |  548 | `		return iExitStatus;` |
|     - |  549 | `	}` |
|  1730 |  550 | `}` |
|     - |  551 |  |
