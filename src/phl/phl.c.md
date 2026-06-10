# src/phl/phl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 130/167 lines (77.84%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `/*` |
|    - |    7 | ` * The PHL interpreter is a simple stand-alone PHP interpreter that allows` |
|    - |    8 | ` * the user to enter and execute PHP files against a PH7 engine.` |
|    - |    9 | ` * To start the phl program, just type "phl" followed by the name of the PHP file` |
|    - |   10 | ` * to compile and execute. That is, the first argument is to the interpreter, the rest` |
|    - |   11 | ` * are scripts arguments, press "Enter" and the PHP code will be executed.` |
|    - |   12 | ` * If something goes wrong while processing the PHP script due to a compile-time error` |
|    - |   13 | ` * your error output (STDOUT) should display the compile-time error messages.` |
|    - |   14 | ` *` |
|    - |   15 | ` * Usage example of the phl interpreter:` |
|    - |   16 | ` *   phl examples/hello_world.php` |
|    - |   17 | ` * Running the interpreter with script arguments` |
|    - |   18 | ` *    phl scripts/mp3_tag.php /usr/local/path/to/my_mp3s` |
|    - |   19 | ` *` |
|    - |   20 | ` * Command line options:` |
|    - |   21 | ` *   -b: Dump PH7 byte-code instructions` |
|    - |   22 | ` *   -h: Display this help message` |
|    - |   23 | ` *` |
|    - |   24 | ` * The PHL interpreter package includes more than 70 PHP scripts to test ranging from` |
|    - |   25 | ` * simple hello world programs to XML processing, ZIP archive extracting, MP3 tag extracting,` |
|    - |   26 | ` * UUID generation, JSON encoding/decoding, INI processing, Base32 encoding/decoding and many` |
|    - |   27 | ` * more. These scripts are available in the scripts directory from the zip archive.` |
|    - |   28 | ` */` |
|    - |   29 | `#include <stdio.h>` |
|    - |   30 | `#include <stdlib.h>` |
|    - |   31 | `#include <string.h>` |
|    - |   32 | `/* Make sure this header file is available.*/` |
|    - |   33 | `#include "ph7.h"` |
|    - |   34 | `#ifdef PHL_ENABLE_SERVER` |
|    - |   35 | `#include "server.h"` |
|    - |   36 | `#endif` |
|    - |   37 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|    - |   38 | `#define MINIDUMP_IMPLEMENTATION` |
|    - |   39 | `#include "minidump.h"` |
|    - |   40 | `#endif` |
|    - |   41 | `/*` |
|    - |   42 | ` * Display an error message and exit.` |
|    - |   43 | ` */` |
|  314 |   44 | `static void Fatal(const char *zMsg)` |
|    1 |   45 |  |
|  315 |   46 | `	puts(zMsg);` |
|    - |   47 | `	/* Shutdown the library */` |
|  315 |   48 | `	ph7_lib_shutdown();` |
|    - |   49 | `	/* Exit immediately */` |
|  315 |   50 | `	exit(0);` |
|  ! 0 |   51 |  |
|    - |   52 | `/*` |
|    - |   53 | ` * Display the banner,a help message and exit.` |
|    - |   54 | ` */` |
|    2 |   55 | `static void Help(void)` |
|    1 |   56 |  |
|    3 |   57 | `	puts("phl [-h\|--help\|-b\|-v\|--version\|-r code] path/to/php_file [script args]");` |
|    - |   58 | `#ifdef PHL_ENABLE_SERVER` |
|    3 |   59 | `	puts("phl -S host:port [-t docroot] [router.php]");` |
|    - |   60 | `#endif` |
|    3 |   61 | `	puts("\t-b: Dump PH7 byte-code instructions");` |
|    3 |   62 | `	puts("\t-r code: Run code from command line (no tags needed)");` |
|    - |   63 | `#ifdef PHL_ENABLE_SERVER` |
|    3 |   64 | `	puts("\t-S host:port: Start the built-in development server");` |
|    3 |   65 | `	puts("\t-t docroot: Document root for the server (default: current directory)");` |
|    - |   66 | `#endif` |
|    3 |   67 | `	puts("\t-v, --version: Display version information and exit");` |
|    3 |   68 | `	puts("\t-h, --help: Display this message and exit");` |
|    - |   69 | `	/* Exit immediately */` |
|    3 |   70 | `	exit(0);` |
|  ! 0 |   71 |  |
|    - |   72 | `/*` |
|    - |   73 | ` * Display version information and exit.` |
|    - |   74 | ` */` |
|    2 |   75 | `static void Version(void)` |
|    1 |   76 |  |
|    3 |   77 | `	puts("PHL " PH7_VERSION " (cli) (built " __DATE__ " " __TIME__ ")");` |
|    3 |   78 | `	puts("Copyright (c) 2011-2014 Symisc Systems, 2025 Alexandre Gomes Gaigalas");` |
|    - |   79 | `	/* Exit immediately */` |
|    3 |   80 | `	exit(0);` |
|  ! 0 |   81 |  |
|    - |   82 | `#ifdef __WINNT__` |
|    - |   83 | `#include <Windows.h>` |
|    - |   84 | `#else` |
|    - |   85 | `/* Assume UNIX */` |
|    - |   86 | `#include <unistd.h>` |
|    - |   87 | `#include <limits.h>` |
|    - |   88 | `#endif` |
|    - |   89 | `/*` |
|    - |   90 | ` * The following define is used by the UNIX built and have` |
|    - |   91 | ` * no particular meaning on windows.` |
|    - |   92 | ` */` |
|    - |   93 | `#ifndef STDOUT_FILENO` |
|    - |   94 | `#define STDOUT_FILENO	1` |
|    - |   95 | `#endif` |
|    - |   96 | `#ifndef PATH_MAX` |
|    - |   97 | `#define PATH_MAX 4096` |
|    - |   98 | `#endif` |
|    - |   99 | `static char zPhlBinaryPath[PATH_MAX];` |
|    - |  100 | `/*` |
|    - |  101 | ` * Expand callback for the PHP_BINARY constant.` |
|    - |  102 | ` * pUserData points to the resolved binary path.` |
|    - |  103 | ` */` |
|    2 |  104 | `static void PHL_PhpBinaryConst(ph7_value *pVal,void *pUserData)` |
|    1 |  105 |  |
|    3 |  106 | `	ph7_value_string(pVal,(const char *)pUserData,-1);` |
|    3 |  107 |  |
|    - |  108 | `/*` |
|    - |  109 | ` * Resolve the absolute path of the running interpreter.` |
|    - |  110 | ` * Falls back to argv[0] verbatim (e.g. bare PATH invocation):` |
|    - |  111 | ` * consumers spawning it again go through the shell, which re-resolves it.` |
|    - |  112 | ` */` |
| 2804 |  113 | `static const char * PHL_ResolveBinaryPath(const char *zArgv0)` |
|    2 |  114 |  |
|    - |  115 | `#ifdef __WINNT__` |
|    2 |  116 | `	DWORD nLen = GetModuleFileNameA(0,zPhlBinaryPath,(DWORD)sizeof(zPhlBinaryPath));` |
|    2 |  117 | `	if( nLen > 0 && nLen < sizeof(zPhlBinaryPath) ){` |
|    2 |  118 | `		return zPhlBinaryPath;` |
|    - |  119 | `	}` |
|    - |  120 | `#else` |
| 2804 |  121 | `	if( realpath(zArgv0,zPhlBinaryPath) != 0 ){` |
| 2804 |  122 | `		return zPhlBinaryPath;` |
|    - |  123 | `	}` |
|    - |  124 | `#endif` |
|  ! 0 |  125 | `	return zArgv0;` |
| 1404 |  126 |  |
|    - |  127 | `/*` |
|    - |  128 | ` * VM output consumer callback.` |
|    - |  129 | ` * Each time the virtual machine generates some outputs,the following` |
|    - |  130 | ` * function gets called by the underlying virtual machine to consume` |
|    - |  131 | ` * the generated output.` |
|    - |  132 | ` * All this function does is redirecting the VM output to STDOUT.` |
|    - |  133 | ` * This function is registered later via a call to ph7_vm_config()` |
|    - |  134 | ` * with a configuration verb set to: PH7_VM_CONFIG_OUTPUT.` |
|    - |  135 | ` */` |
| 8856 |  136 | `static int Output_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)` |
|    2 |  137 |  |
| 4428 |  138 | `	(void)pUserData;` |
|    - |  139 | `#ifdef __WINNT__` |
|    - |  140 | `	BOOL rc;` |
|    2 |  141 | `	rc = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),pOutput,(DWORD)nOutputLen,0,0);` |
|    2 |  142 | `	if( !rc ){` |
|    - |  143 | `		/* Abort processing */` |
|  ! 0 |  144 | `		return PH7_ABORT;` |
|    - |  145 | `	}` |
|    - |  146 | `#else` |
|    - |  147 | `	ssize_t nWr;` |
| 8856 |  148 | `	nWr = write(STDOUT_FILENO,pOutput,nOutputLen);` |
| 8856 |  149 | `	if( nWr < 0 ){` |
|    - |  150 | `		/* Abort processing */` |
|  ! 0 |  151 | `		return PH7_ABORT;` |
|    - |  152 | `	}` |
|    - |  153 | `#endif /* __WINT__ */` |
|    - |  154 | `	/* All done,VM output was redirected to STDOUT */` |
| 8858 |  155 | `	return PH7_OK;` |
| 4430 |  156 |  |
|    - |  157 | `/*` |
|    - |  158 | ` * Main program: Compile and execute the PHP file.` |
|    - |  159 | ` */` |
| 3271 |  160 | `int main(int argc,char **argv)` |
|    2 |  161 |  |
|    - |  162 | `	ph7 *pEngine; /* PH7 engine */` |
|    - |  163 | `	ph7_vm *pVm;  /* Compiled PHP program */` |
| 3273 |  164 | `	int dump_vm = 0;    /* Dump VM instructions if TRUE */` |
| 3273 |  165 | `	int run_code = 0;    /* Run inline code if TRUE */` |
| 3273 |  166 | `	const char *zRunCode = 0; /* Inline code string */` |
|    - |  167 | `#ifdef PHL_ENABLE_SERVER` |
| 3273 |  168 | `	int server_mode = 0;        /* Start built-in server if TRUE */` |
| 3273 |  169 | `	const char *zServerAddr = 0; /* host:port string */` |
| 3273 |  170 | `	const char *zDocRoot = ".";  /* Document root */` |
|    - |  171 | `#endif` |
|    - |  172 | `	int n;              /* Script arguments */` |
|    - |  173 | `	int rc;` |
|    - |  174 | `	/* Process interpreter arguments first*/` |
| 3321 |  175 | `	for(n = 1 ; n < argc ; ++n ){` |
|    - |  176 | `		int c;` |
| 3164 |  177 | `		if( argv[n][0] != '-' ){` |
|    - |  178 | `			/* No more interpreter arguments */` |
| 3114 |  179 | `			break;` |
|    - |  180 | `		}` |
|    - |  181 | `		/* Check for long options */` |
|   51 |  182 | `		if( argv[n][1] == '-' ){` |
|    5 |  183 | `			if( strcmp(argv[n], "--version") == 0 ){` |
|    3 |  184 | `				Version();` |
|    4 |  185 | `			}else if( strcmp(argv[n], "--help") == 0 ){` |
|    3 |  186 | `				Help();` |
|    1 |  187 | `			}else{` |
|    - |  188 | `				/* Unknown long option */` |
|  ! 0 |  189 | `				Help();` |
|    - |  190 | `			}` |
|    2 |  191 | `			continue;` |
|    - |  192 | `		}` |
|   47 |  193 | `		c = argv[n][1];` |
|   47 |  194 | `		if( c == 'b' ){` |
|    - |  195 | `			/* Dump byte-code instructions */` |
|    3 |  196 | `			dump_vm = 1;` |
|   46 |  197 | `		}else if( c == 'r' ){` |
|    - |  198 | `			/* Run inline PHP code from next argument (php -r style) */` |
|    9 |  199 | `			if( n + 1 >= argc ){` |
|    - |  200 | `				/* Missing code argument */` |
|  ! 0 |  201 | `				Fatal("Missing code argument for -r");` |
|  ! 0 |  202 | `			}` |
|    9 |  203 | `			zRunCode = argv[++n];` |
|    9 |  204 | `			run_code = 1;` |
|   40 |  205 | `		}else if( c == 'S' ){` |
|    - |  206 | `			/* Start built-in development server */` |
|    - |  207 | `#ifdef PHL_ENABLE_SERVER` |
|   18 |  208 | `			if( n + 1 >= argc ){` |
|  ! 0 |  209 | `				Fatal("Missing host:port argument for -S");` |
|  ! 0 |  210 | `			}` |
|   18 |  211 | `			zServerAddr = argv[++n];` |
|   18 |  212 | `			server_mode = 1;` |
|    - |  213 | `#else` |
|    - |  214 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|    - |  215 | `#endif` |
|   27 |  216 | `		}else if( c == 't' ){` |
|    - |  217 | `			/* Set document root for the server */` |
|    - |  218 | `#ifdef PHL_ENABLE_SERVER` |
|   18 |  219 | `			if( n + 1 >= argc ){` |
|  ! 0 |  220 | `				Fatal("Missing docroot argument for -t");` |
|  ! 0 |  221 | `			}` |
|   18 |  222 | `			zDocRoot = argv[++n];` |
|    - |  223 | `#else` |
|    - |  224 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|    - |  225 | `#endif` |
|    9 |  226 | `		}else if( c == 'v' ){` |
|    - |  227 | `			/* Display version */` |
|  ! 0 |  228 | `			Version();` |
|  ! 0 |  229 | `		}else{` |
|    - |  230 | `			/* Display a help message and exit */` |
|  ! 0 |  231 | `			Help();` |
|    - |  232 | `		}` |
|   24 |  233 | `	}` |
|    - |  234 | `#ifdef PHL_ENABLE_SERVER` |
| 3271 |  235 | `	if( server_mode ){` |
|    - |  236 | `		/* Parse host:port from zServerAddr */` |
|    - |  237 | `		char zHost[256];` |
|   18 |  238 | `		int iPort = 0;` |
|    - |  239 | `		const char *zColon;` |
|   18 |  240 | `		const char *zRouter = 0;` |
|   18 |  241 | `		zColon = strrchr(zServerAddr, ':');` |
|   18 |  242 | `		if( zColon == 0 ){` |
|  ! 0 |  243 | `			Fatal("Invalid address format. Use host:port (e.g., localhost:8080)");` |
|  ! 0 |  244 | `		}` |
|    - |  245 | `		{` |
|   18 |  246 | `			int nHostLen = (int)(zColon - zServerAddr);` |
|   18 |  247 | `			if( nHostLen >= (int)sizeof(zHost) ) nHostLen = (int)sizeof(zHost) - 1;` |
|   18 |  248 | `			memcpy(zHost, zServerAddr, nHostLen);` |
|   18 |  249 | `			zHost[nHostLen] = 0;` |
|    - |  250 | `		}` |
|   18 |  251 | `		iPort = atoi(zColon + 1);` |
|   18 |  252 | `		if( iPort <= 0 \|\| iPort > 65535 ){` |
|  ! 0 |  253 | `			Fatal("Invalid port number");` |
|  ! 0 |  254 | `		}` |
|    - |  255 | `		/* Check for optional router script */` |
|   18 |  256 | `		if( n < argc ){` |
|  ! 0 |  257 | `			zRouter = argv[n];` |
|  ! 0 |  258 | `		}` |
|   18 |  259 | `		return phl_serve(zHost, iPort, zDocRoot, zRouter);` |
|    - |  260 | `	}` |
|    - |  261 | `#endif` |
| 2963 |  262 | `	if( n >= argc && !run_code ){` |
|  ! 0 |  263 | `		puts("Missing PHP file to compile");` |
|  ! 0 |  264 | `		Help();` |
|  ! 0 |  265 | `	}` |
|    - |  266 |  |
|    - |  267 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|    - |  268 | `	/* Install an unhandled exception minidump handler for Windows debug builds */` |
|    2 |  269 | `	CreateMiniDumpOnUnHandledException();` |
|    - |  270 | `#endif` |
|    - |  271 | `	/* Allocate a new PH7 engine instance */` |
| 2963 |  272 | `	rc = ph7_init(&pEngine);` |
| 2963 |  273 | `	if( rc != PH7_OK ){` |
|    - |  274 | `		/*` |
|    - |  275 | `		 * If the supplied memory subsystem is so sick that we are unable` |
|    - |  276 | `		 * to allocate a tiny chunk of memory,there is no much we can do here.` |
|    - |  277 | `		 */` |
|  ! 0 |  278 | `		Fatal("Error while allocating a new PH7 engine instance");` |
|  ! 0 |  279 | `	}` |
|    - |  280 | `	/* Set an error log consumer callback. This callback [Output_Consumer()] will` |
|    - |  281 | `	 * redirect all compile-time error messages to STDOUT.` |
|    - |  282 | `	 */` |
| 2963 |  283 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,` |
|    - |  284 | `		Output_Consumer, /* Error log consumer */` |
|    - |  285 |  |
|    - |  286 | `		);` |
|    - |  287 | `	/* Now,it's time to compile our PHP file */` |
| 2963 |  288 | `	if( run_code ){` |
|    - |  289 | `		/* Compile inline PHP code string (PHP only - no tags needed) */` |
|    9 |  290 | `		rc = ph7_compile_v2(` |
|    4 |  291 | `			pEngine, /* PH7 Engine */` |
|    4 |  292 | `			zRunCode, /* Source code */` |
|    - |  293 | `			-1,       /* Let API compute length */` |
|    - |  294 | `			&pVm,     /* OUT: Compiled PHP program */` |
|    - |  295 | `			PH7_PHP_ONLY /* Inline PHP, no tags expected */` |
|    - |  296 | `			);` |
|    9 |  297 | `		if( rc != PH7_OK ){ /* Compile error */` |
|  ! 0 |  298 | `			if( rc == PH7_VM_ERR ){` |
|  ! 0 |  299 | `				Fatal("VM initialization error");` |
|  ! 0 |  300 | `			}else{` |
|    - |  301 | `				/* Compile-time error, your output (STDOUT) should display the error messages */` |
|  ! 0 |  302 | `				Fatal("Compile error");` |
|    - |  303 | `			}` |
|  ! 0 |  304 | `		}` |
|    5 |  305 | `	}else{` |
| 2955 |  306 | `		rc = ph7_compile_file(` |
| 1398 |  307 | `			pEngine, /* PH7 Engine */` |
| 2953 |  308 | `			argv[n], /* Path to the PHP file to compile */` |
|    - |  309 | `			&pVm,    /* OUT: Compiled PHP program */` |
|    - |  310 |  |
|    - |  311 | `			);` |
| 2955 |  312 | `		if( rc != PH7_OK ){ /* Compile error */` |
|  315 |  313 | `			if( rc == PH7_IO_ERR ){` |
|  ! 0 |  314 | `				Fatal("IO error while opening the target file");` |
|  315 |  315 | `			}else if( rc == PH7_VM_ERR ){` |
|  ! 0 |  316 | `				Fatal("VM initialization error");` |
|  ! 0 |  317 | `			}else{` |
|    - |  318 | `				/* Compile-time error, your output (STDOUT) should display the error messages */` |
|  315 |  319 | `				Fatal("Compile error");` |
|    - |  320 | `			}` |
|  157 |  321 | `		}` |
|    - |  322 | `	}` |
|    - |  323 | `	/*` |
|    - |  324 | `	 * Now we have our script compiled,it's time to configure our VM.` |
|    - |  325 | `	 * We will install the VM output consumer callback defined above` |
|    - |  326 | `	 * so that we can consume the VM output and redirect it to STDOUT.` |
|    - |  327 | `	 */` |
| 2806 |  328 | `	rc = ph7_vm_config(pVm,` |
|    - |  329 | `		PH7_VM_CONFIG_OUTPUT,` |
|    - |  330 | `		Output_Consumer,    /* Output Consumer callback */` |
|    - |  331 |  |
|    - |  332 | `		);` |
| 2806 |  333 | `	if( rc != PH7_OK ){` |
|  ! 0 |  334 | `		Fatal("Error while installing the VM output consumer callback");` |
|  ! 0 |  335 | `	}` |
|    - |  336 | `	/* Define PHP_BINARY: absolute path of this interpreter */` |
| 4208 |  337 | `	ph7_create_constant(pVm,"PHP_BINARY",PHL_PhpBinaryConst,` |
| 2804 |  338 | `		(void *)PHL_ResolveBinaryPath(argv[0]));` |
|    - |  339 | `	/* Register script arguments so we can access them later using the $argv[]` |
|    - |  340 | `	 * array from the compiled PHP program. For regular file execution we need` |
|    - |  341 | `	 * to register the arguments after the script file, while for inline code` |
|    - |  342 | `	 * (-r) the arguments start at the current index.` |
|    - |  343 | `	 */` |
| 2806 |  344 | `	if( run_code ){` |
|   11 |  345 | `		for( ; n < argc ; ++n ){` |
|    2 |  346 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,argv[n]/* Argument value */);` |
|    1 |  347 | `		}` |
|    5 |  348 | `	}else{` |
| 2818 |  349 | `		for( n = n + 1; n < argc ; ++n ){` |
|   22 |  350 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,argv[n]/* Argument value */);` |
|   12 |  351 | `		}` |
|    - |  352 | `	}` |
|    - |  353 | `	/* Report script run-time errors (now default behavior) */` |
| 2806 |  354 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_ERR_REPORT);` |
| 2806 |  355 | `	if( dump_vm ){` |
|    - |  356 | `		/* Dump PH7 byte-code instructions */` |
|    3 |  357 | `		ph7_vm_dump_v2(pVm,` |
|    - |  358 | `			Output_Consumer, /* Dump consumer callback */` |
|    - |  359 |  |
|    - |  360 | `			);` |
|    1 |  361 | `	}` |
|    - |  362 | `	/*` |
|    - |  363 | `	 * And finally, execute our program. Note that your output (STDOUT in our case)` |
|    - |  364 | `	 * should display the result.` |
|    - |  365 | `	 */` |
|    - |  366 | `	{` |
| 2806 |  367 | `		int iExitStatus = 0;` |
| 2806 |  368 | `		ph7_vm_exec(pVm,&iExitStatus);` |
|    - |  369 | `		/* All done, cleanup the mess left behind.` |
|    - |  370 | `		*/` |
| 2806 |  371 | `		ph7_vm_release(pVm);` |
| 2806 |  372 | `		ph7_release(pEngine);` |
|    - |  373 | `		/* Propagate the script exit status (set via exit()/die()) */` |
| 2806 |  374 | `		return iExitStatus;` |
|    - |  375 | `	}` |
| 1413 |  376 |  |
|    - |  377 |  |
