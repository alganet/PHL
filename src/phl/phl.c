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
 * Display the banner,a help message and exit.
 */
static void Help(void)
{
	puts("phl [-h|--help|-b|-i|-l|-v|--version|-r code] path/to/php_file [script args]");
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
 * Parse an unsigned-long testing knob from the environment (PHL_MAX_ALLOC /
 * PHL_MAX_INPUT / PHL_MAX_RECURSION). Returns 1 and writes *pOut on a valid,
 * strictly-positive, fully-numeric value clamped to [uFloor, uCeil]; returns
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
#ifdef PHL_ENABLE_SERVER
	int server_mode = 0;        /* Start built-in server if TRUE */
	const char *zServerAddr = 0; /* host:port string */
	const char *zDocRoot = ".";  /* Document root */
#endif
	int n;              /* Script arguments */
	int rc;
	/* Process interpreter arguments first*/
	for(n = 1 ; n < argc ; ++n ){
		int c;
		if( argv[n][0] != '-' ){
			/* No more interpreter arguments */
			break;
		}
		/* Check for long options */
		if( argv[n][1] == '-' ){
			if( strcmp(argv[n], "--version") == 0 ){
				Version();
			}else if( strcmp(argv[n], "--help") == 0 ){
				Help();
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
	if( n >= argc && !run_code ){
		puts("Missing PHP file to compile");
		Help();
	}

#if defined(__WINNT__) && defined(PH7_DEBUG)
	/* Install an unhandled exception minidump handler for Windows debug builds */
	CreateMiniDumpOnUnHandledException();
#endif
	/* Allocate a new PH7 engine instance */
	rc = ph7_init(&pEngine);
	if( rc != PH7_OK ){
		/*
		 * If the supplied memory subsystem is so sick that we are unable
		 * to allocate a tiny chunk of memory,there is no much we can do here.
		 */
		Fatal("Error while allocating a new PH7 engine instance");
	}
	/* Set an error log consumer callback. This callback [Output_Consumer()] will
	 * redirect all compile-time error messages to STDOUT.
	 */
	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,
		Output_Consumer, /* Error log consumer */
		0 /* NULL: Callback Private data */
		);
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
			puts("No input file specified");
			return 255;
		}
		zFile = argv[n];
		rc = ph7_compile_file(pEngine,zFile,&pVm,0);
		if( rc == PH7_OK ){
			printf("No syntax errors detected in %s\n",zFile);
			ph7_vm_release(pVm);
		}else if( rc == PH7_IO_ERR ){
			printf("Could not open input file: %s\n",zFile);
		}else{
			printf("Errors parsing %s\n",zFile);
		}
		ph7_release(pEngine);
		return (rc == PH7_OK) ? 0 : 255;
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
				/* Compile-time error, your output (STDOUT) should display the error messages */
				Fatal("Compile error");
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
				FatalCode("IO error while opening the target file",1);
			}else if( rc == PH7_VM_ERR ){
				Fatal("VM initialization error");
			}else{
				/* Compile-time error, your output (STDOUT) should display the error messages */
				Fatal("Compile error");
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
	/* Optional PHP-recursion-depth cap override (PHL_MAX_RECURSION=frames).
	 * Testing hatch like PHL_MAX_ALLOC: lets the deep-recursion tests
	 * (tests/ph7/004-deep) run past the conservative host default until the
	 * BYTECODE.md stage-5 config rework makes the host default unbounded.
	 * Floor of 3 because PH7_VM_CONFIG_RECURSION_DEPTH ignores nDepth<=2. */
	{
		unsigned long uMax;
		if( PHL_EnvULong("PHL_MAX_RECURSION",3UL,0x7FFFFFFFUL,&uMax) ){
			ph7_vm_config(pVm,PH7_VM_CONFIG_RECURSION_DEPTH,(int)uMax);
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
		const char *zScriptName = run_code ? "Standard input code" : argv[n];
		int argv_count = 0;
		ph7_value *pArgc;
		/* Count only the entries actually inserted: PH7_VM_CONFIG_ARGV_ENTRY skips
		 * an empty string, so counting unconditionally would leave $argc greater
		 * than count($argv) for an empty argument (e.g. `phl s.php "" x`). */
		if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,zScriptName) == PH7_OK ){
			argv_count++;
		}
		/* The script's own arguments follow: in file mode argv[n] is the script
		 * (registered above), so they start at n+1; in -r mode they start at n. */
		for( n = run_code ? n : n + 1; n < argc ; ++n ){
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
	/* Report script run-time errors (now default behavior) */
	ph7_vm_config(pVm,PH7_VM_CONFIG_ERR_REPORT);
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
		/* Propagate the script exit status (set via exit()/die()) */
		return iExitStatus;
	}
}
