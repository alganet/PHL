# src/phl/phl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 115/151 lines (76.16%)

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
|  260 |   44 | `static void Fatal(const char *zMsg)` |
|    1 |   45 |  |
|  261 |   46 | `	puts(zMsg);` |
|    - |   47 | `	/* Shutdown the library */` |
|  261 |   48 | `	ph7_lib_shutdown();` |
|    - |   49 | `	/* Exit immediately */` |
|  261 |   50 | `	exit(0);` |
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
|    - |   87 | `#endif` |
|    - |   88 | `/*` |
|    - |   89 | ` * The following define is used by the UNIX built and have` |
|    - |   90 | ` * no particular meaning on windows.` |
|    - |   91 | ` */` |
|    - |   92 | `#ifndef STDOUT_FILENO` |
|    - |   93 | `#define STDOUT_FILENO	1` |
|    - |   94 | `#endif` |
|    - |   95 | `/*` |
|    - |   96 | ` * VM output consumer callback.` |
|    - |   97 | ` * Each time the virtual machine generates some outputs,the following` |
|    - |   98 | ` * function gets called by the underlying virtual machine to consume` |
|    - |   99 | ` * the generated output.` |
|    - |  100 | ` * All this function does is redirecting the VM output to STDOUT.` |
|    - |  101 | ` * This function is registered later via a call to ph7_vm_config()` |
|    - |  102 | ` * with a configuration verb set to: PH7_VM_CONFIG_OUTPUT.` |
|    - |  103 | ` */` |
| 6786 |  104 | `static int Output_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)` |
|    2 |  105 |  |
| 3393 |  106 | `	(void)pUserData;` |
|    - |  107 | `#ifdef __WINNT__` |
|    - |  108 | `	BOOL rc;` |
|    2 |  109 | `	rc = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),pOutput,(DWORD)nOutputLen,0,0);` |
|    2 |  110 | `	if( !rc ){` |
|    - |  111 | `		/* Abort processing */` |
|  ! 0 |  112 | `		return PH7_ABORT;` |
|    - |  113 | `	}` |
|    - |  114 | `#else` |
|    - |  115 | `	ssize_t nWr;` |
| 6786 |  116 | `	nWr = write(STDOUT_FILENO,pOutput,nOutputLen);` |
| 6786 |  117 | `	if( nWr < 0 ){` |
|    - |  118 | `		/* Abort processing */` |
|  ! 0 |  119 | `		return PH7_ABORT;` |
|    - |  120 | `	}` |
|    - |  121 | `#endif /* __WINT__ */` |
|    - |  122 | `	/* All done,VM output was redirected to STDOUT */` |
| 6788 |  123 | `	return PH7_OK;` |
| 3395 |  124 |  |
|    - |  125 | `/*` |
|    - |  126 | ` * Main program: Compile and execute the PHP file.` |
|    - |  127 | ` */` |
| 2674 |  128 | `int main(int argc,char **argv)` |
|    2 |  129 |  |
|    - |  130 | `	ph7 *pEngine; /* PH7 engine */` |
|    - |  131 | `	ph7_vm *pVm;  /* Compiled PHP program */` |
| 2676 |  132 | `	int dump_vm = 0;    /* Dump VM instructions if TRUE */` |
| 2676 |  133 | `	int run_code = 0;    /* Run inline code if TRUE */` |
| 2676 |  134 | `	const char *zRunCode = 0; /* Inline code string */` |
|    - |  135 | `#ifdef PHL_ENABLE_SERVER` |
| 2676 |  136 | `	int server_mode = 0;        /* Start built-in server if TRUE */` |
| 2676 |  137 | `	const char *zServerAddr = 0; /* host:port string */` |
| 2676 |  138 | `	const char *zDocRoot = ".";  /* Document root */` |
|    - |  139 | `#endif` |
|    - |  140 | `	int n;              /* Script arguments */` |
|    - |  141 | `	int rc;` |
|    - |  142 | `	/* Process interpreter arguments first*/` |
| 2724 |  143 | `	for(n = 1 ; n < argc ; ++n ){` |
|    - |  144 | `		int c;` |
| 2594 |  145 | `		if( argv[n][0] != '-' ){` |
|    - |  146 | `			/* No more interpreter arguments */` |
| 2544 |  147 | `			break;` |
|    - |  148 | `		}` |
|    - |  149 | `		/* Check for long options */` |
|   51 |  150 | `		if( argv[n][1] == '-' ){` |
|    5 |  151 | `			if( strcmp(argv[n], "--version") == 0 ){` |
|    3 |  152 | `				Version();` |
|    4 |  153 | `			}else if( strcmp(argv[n], "--help") == 0 ){` |
|    3 |  154 | `				Help();` |
|    1 |  155 | `			}else{` |
|    - |  156 | `				/* Unknown long option */` |
|  ! 0 |  157 | `				Help();` |
|    - |  158 | `			}` |
|    2 |  159 | `			continue;` |
|    - |  160 | `		}` |
|   47 |  161 | `		c = argv[n][1];` |
|   47 |  162 | `		if( c == 'b' ){` |
|    - |  163 | `			/* Dump byte-code instructions */` |
|    3 |  164 | `			dump_vm = 1;` |
|   46 |  165 | `		}else if( c == 'r' ){` |
|    - |  166 | `			/* Run inline PHP code from next argument (php -r style) */` |
|    9 |  167 | `			if( n + 1 >= argc ){` |
|    - |  168 | `				/* Missing code argument */` |
|  ! 0 |  169 | `				Fatal("Missing code argument for -r");` |
|  ! 0 |  170 | `			}` |
|    9 |  171 | `			zRunCode = argv[++n];` |
|    9 |  172 | `			run_code = 1;` |
|   40 |  173 | `		}else if( c == 'S' ){` |
|    - |  174 | `			/* Start built-in development server */` |
|    - |  175 | `#ifdef PHL_ENABLE_SERVER` |
|   18 |  176 | `			if( n + 1 >= argc ){` |
|  ! 0 |  177 | `				Fatal("Missing host:port argument for -S");` |
|  ! 0 |  178 | `			}` |
|   18 |  179 | `			zServerAddr = argv[++n];` |
|   18 |  180 | `			server_mode = 1;` |
|    - |  181 | `#else` |
|    - |  182 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|    - |  183 | `#endif` |
|   27 |  184 | `		}else if( c == 't' ){` |
|    - |  185 | `			/* Set document root for the server */` |
|    - |  186 | `#ifdef PHL_ENABLE_SERVER` |
|   18 |  187 | `			if( n + 1 >= argc ){` |
|  ! 0 |  188 | `				Fatal("Missing docroot argument for -t");` |
|  ! 0 |  189 | `			}` |
|   18 |  190 | `			zDocRoot = argv[++n];` |
|    - |  191 | `#else` |
|    - |  192 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|    - |  193 | `#endif` |
|    9 |  194 | `		}else if( c == 'v' ){` |
|    - |  195 | `			/* Display version */` |
|  ! 0 |  196 | `			Version();` |
|  ! 0 |  197 | `		}else{` |
|    - |  198 | `			/* Display a help message and exit */` |
|  ! 0 |  199 | `			Help();` |
|    - |  200 | `		}` |
|   24 |  201 | `	}` |
|    - |  202 | `#ifdef PHL_ENABLE_SERVER` |
| 2674 |  203 | `	if( server_mode ){` |
|    - |  204 | `		/* Parse host:port from zServerAddr */` |
|    - |  205 | `		char zHost[256];` |
|   18 |  206 | `		int iPort = 0;` |
|    - |  207 | `		const char *zColon;` |
|   18 |  208 | `		const char *zRouter = 0;` |
|   18 |  209 | `		zColon = strrchr(zServerAddr, ':');` |
|   18 |  210 | `		if( zColon == 0 ){` |
|  ! 0 |  211 | `			Fatal("Invalid address format. Use host:port (e.g., localhost:8080)");` |
|  ! 0 |  212 | `		}` |
|    - |  213 | `		{` |
|   18 |  214 | `			int nHostLen = (int)(zColon - zServerAddr);` |
|   18 |  215 | `			if( nHostLen >= (int)sizeof(zHost) ) nHostLen = (int)sizeof(zHost) - 1;` |
|   18 |  216 | `			memcpy(zHost, zServerAddr, nHostLen);` |
|   18 |  217 | `			zHost[nHostLen] = 0;` |
|    - |  218 | `		}` |
|   18 |  219 | `		iPort = atoi(zColon + 1);` |
|   18 |  220 | `		if( iPort <= 0 \|\| iPort > 65535 ){` |
|  ! 0 |  221 | `			Fatal("Invalid port number");` |
|  ! 0 |  222 | `		}` |
|    - |  223 | `		/* Check for optional router script */` |
|   18 |  224 | `		if( n < argc ){` |
|  ! 0 |  225 | `			zRouter = argv[n];` |
|  ! 0 |  226 | `		}` |
|   18 |  227 | `		return phl_serve(zHost, iPort, zDocRoot, zRouter);` |
|    - |  228 | `	}` |
|    - |  229 | `#endif` |
| 2420 |  230 | `	if( n >= argc && !run_code ){` |
|  ! 0 |  231 | `		puts("Missing PHP file to compile");` |
|  ! 0 |  232 | `		Help();` |
|  ! 0 |  233 | `	}` |
|    - |  234 |  |
|    - |  235 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|    - |  236 | `	/* Install an unhandled exception minidump handler for Windows debug builds */` |
|    2 |  237 | `	CreateMiniDumpOnUnHandledException();` |
|    - |  238 | `#endif` |
|    - |  239 | `	/* Allocate a new PH7 engine instance */` |
| 2420 |  240 | `	rc = ph7_init(&pEngine);` |
| 2420 |  241 | `	if( rc != PH7_OK ){` |
|    - |  242 | `		/*` |
|    - |  243 | `		 * If the supplied memory subsystem is so sick that we are unable` |
|    - |  244 | `		 * to allocate a tiny chunk of memory,there is no much we can do here.` |
|    - |  245 | `		 */` |
|  ! 0 |  246 | `		Fatal("Error while allocating a new PH7 engine instance");` |
|  ! 0 |  247 | `	}` |
|    - |  248 | `	/* Set an error log consumer callback. This callback [Output_Consumer()] will` |
|    - |  249 | `	 * redirect all compile-time error messages to STDOUT.` |
|    - |  250 | `	 */` |
| 2420 |  251 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,` |
|    - |  252 | `		Output_Consumer, /* Error log consumer */` |
|    - |  253 |  |
|    - |  254 | `		);` |
|    - |  255 | `	/* Now,it's time to compile our PHP file */` |
| 2420 |  256 | `	if( run_code ){` |
|    - |  257 | `		/* Compile inline PHP code string (PHP only - no tags needed) */` |
|    9 |  258 | `		rc = ph7_compile_v2(` |
|    4 |  259 | `			pEngine, /* PH7 Engine */` |
|    4 |  260 | `			zRunCode, /* Source code */` |
|    - |  261 | `			-1,       /* Let API compute length */` |
|    - |  262 | `			&pVm,     /* OUT: Compiled PHP program */` |
|    - |  263 | `			PH7_PHP_ONLY /* Inline PHP, no tags expected */` |
|    - |  264 | `			);` |
|    9 |  265 | `		if( rc != PH7_OK ){ /* Compile error */` |
|  ! 0 |  266 | `			if( rc == PH7_VM_ERR ){` |
|  ! 0 |  267 | `				Fatal("VM initialization error");` |
|  ! 0 |  268 | `			}else{` |
|    - |  269 | `				/* Compile-time error, your output (STDOUT) should display the error messages */` |
|  ! 0 |  270 | `				Fatal("Compile error");` |
|    - |  271 | `			}` |
|  ! 0 |  272 | `		}` |
|    5 |  273 | `	}else{` |
| 2412 |  274 | `		rc = ph7_compile_file(` |
| 1140 |  275 | `			pEngine, /* PH7 Engine */` |
| 2410 |  276 | `			argv[n], /* Path to the PHP file to compile */` |
|    - |  277 | `			&pVm,    /* OUT: Compiled PHP program */` |
|    - |  278 |  |
|    - |  279 | `			);` |
| 2412 |  280 | `		if( rc != PH7_OK ){ /* Compile error */` |
|  261 |  281 | `			if( rc == PH7_IO_ERR ){` |
|  ! 0 |  282 | `				Fatal("IO error while opening the target file");` |
|  261 |  283 | `			}else if( rc == PH7_VM_ERR ){` |
|  ! 0 |  284 | `				Fatal("VM initialization error");` |
|  ! 0 |  285 | `			}else{` |
|    - |  286 | `				/* Compile-time error, your output (STDOUT) should display the error messages */` |
|  261 |  287 | `				Fatal("Compile error");` |
|    - |  288 | `			}` |
|  130 |  289 | `		}` |
|    - |  290 | `	}` |
|    - |  291 | `	/*` |
|    - |  292 | `	 * Now we have our script compiled,it's time to configure our VM.` |
|    - |  293 | `	 * We will install the VM output consumer callback defined above` |
|    - |  294 | `	 * so that we can consume the VM output and redirect it to STDOUT.` |
|    - |  295 | `	 */` |
| 2290 |  296 | `	rc = ph7_vm_config(pVm,` |
|    - |  297 | `		PH7_VM_CONFIG_OUTPUT,` |
|    - |  298 | `		Output_Consumer,    /* Output Consumer callback */` |
|    - |  299 |  |
|    - |  300 | `		);` |
| 2290 |  301 | `	if( rc != PH7_OK ){` |
|  ! 0 |  302 | `		Fatal("Error while installing the VM output consumer callback");` |
|  ! 0 |  303 | `	}` |
|    - |  304 | `	/* Register script arguments so we can access them later using the $argv[]` |
|    - |  305 | `	 * array from the compiled PHP program. For regular file execution we need` |
|    - |  306 | `	 * to register the arguments after the script file, while for inline code` |
|    - |  307 | `	 * (-r) the arguments start at the current index.` |
|    - |  308 | `	 */` |
| 2290 |  309 | `	if( run_code ){` |
|   11 |  310 | `		for( ; n < argc ; ++n ){` |
|    2 |  311 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,argv[n]/* Argument value */);` |
|    1 |  312 | `		}` |
|    5 |  313 | `	}else{` |
| 2302 |  314 | `		for( n = n + 1; n < argc ; ++n ){` |
|   22 |  315 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,argv[n]/* Argument value */);` |
|   12 |  316 | `		}` |
|    - |  317 | `	}` |
|    - |  318 | `	/* Report script run-time errors (now default behavior) */` |
| 2290 |  319 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_ERR_REPORT);` |
| 2290 |  320 | `	if( dump_vm ){` |
|    - |  321 | `		/* Dump PH7 byte-code instructions */` |
|    3 |  322 | `		ph7_vm_dump_v2(pVm,` |
|    - |  323 | `			Output_Consumer, /* Dump consumer callback */` |
|    - |  324 |  |
|    - |  325 | `			);` |
|    1 |  326 | `	}` |
|    - |  327 | `	/*` |
|    - |  328 | `	 * And finally, execute our program. Note that your output (STDOUT in our case)` |
|    - |  329 | `	 * should display the result.` |
|    - |  330 | `	 */` |
| 2290 |  331 | `	ph7_vm_exec(pVm,0);` |
|    - |  332 | `	/* All done, cleanup the mess left behind.` |
|    - |  333 | `	*/` |
| 2286 |  334 | `	ph7_vm_release(pVm);` |
| 2286 |  335 | `	ph7_release(pEngine);` |
| 2286 |  336 | `	return 0;` |
| 1155 |  337 |  |
|    - |  338 |  |
