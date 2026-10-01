# src/ph7/vm_builtin_lib.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 459/526 lines (87.26%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/*` |
|       - |    8 | ` * The embedded PHP source of the core built-in class library (Exception and` |
|       - |    9 | ` * friends, ArrayAccess/Countable/..., Closure, Fiber, Generator shells),` |
|       - |   10 | ` * compiled at VM init by PH7_VmInstallBuiltinLib() — the bootstrap step` |
|       - |   11 | ` * PH7_VmInit runs right after the code generator comes up. Split from vm.c` |
|       - |   12 | ` * so the chunk string and its sizeof stay in one translation unit (the` |
|       - |   13 | ` * vm_builtin_reflection_lib.c pattern).` |
|       - |   14 | ` */` |
|       - |   15 | `/* The eleven core INTERFACES and the whole Exception/Error family are declared` |
|       - |   16 | ` * entirely from C (VmInstallCoreInterfaces / VmInstallExceptions below) -- they` |
|       - |   17 | ` * have no presence in this chunk at all. A chunk cannot express what php declares` |
|       - |   18 | ` * on them: a TENTATIVE return type (php marks nearly every interface method),` |
|       - |   19 | `` * Throwable's `extends Stringable`, the exception family's FINAL getters and`` |
|       - |   20 | ` * private __clone, or a typed slot with no default (Error::$line). */` |
|       - |   21 | `#define PH7_BUILTIN_LIB \` |
|       - |   22 | `	/* Fiber and Generator are declared ENTIRELY from C — class, private slots and` |
|       - |   23 | `	 * every method — by PH7_VmInstallFiberNative / PH7_VmInstallGeneratorNative.` |
|       - |   24 | `	 * They are the first two builtin classes with no presence in this chunk at all.` |
|       - |   25 | ``	 * Generator's `implements Iterator` is attached there too, and has to be: it is`` |
|       - |   26 | `	 * applied AFTER its methods exist, because PH7_ClassImplement installs an` |
|       - |   27 | `	 * ABSTRACT stub for every interface method a class does not already declare. */\` |
|       - |   28 | `	/* Closure is declared ENTIRELY in C by PH7_VmInstallClosureNative() (vm_exec_ctx.c):` |
|       - |   29 | `	 * class, the three engine slots (hidden, as php presents no property) and all five` |
|       - |   30 | `	 * methods. It cannot live here — a chunk-declared property is on every presentation` |
|       - |   31 | ``	 * surface, and `call()` in PHP leaked get_class()'s own TypeError text. */\`` |
|       - |   32 | `	/* stdClass is empty (PHP-exact): holds only dynamic (runtime-added) properties. */\` |
|       - |   33 | `	"function scandir(string $directory,int $sorting_order = SCANDIR_SORT_ASCENDING, $context = null): array\|false"\` |
|       - |   34 | `    "{"\` |
|       - |   35 | `	"  /* php's Z_PARAM_PATH refusal, spelled here because this builtin is prelude PHP:"\` |
|       - |   36 | `	"     the C screen (VmBuiltinPathMask) reaches host builtins only, so without this"\` |
|       - |   37 | `	"     the NUL reached opendir() and the ValueError named opendir(), not scandir(). */"\` |
|       - |   38 | `	"  if( strpos($directory, chr(0)) !== false ){"\` |
|       - |   39 | `	"    throw new ValueError('scandir(): Argument #1 ($directory) must not contain any null bytes');"\` |
|       - |   40 | `	"  }"\` |
|       - |   41 | ``	"  /* ... and its own for an EMPTY one: the stream layer's `Path must not be"\`` |
|       - |   42 | ``	"     empty` is what the file openers raise, not this. */"\`` |
|       - |   43 | `	"  if( $directory === '' ){"\` |
|       - |   44 | `	"    throw new ValueError('scandir(): Argument #1 ($directory) must not be empty');"\` |
|       - |   45 | `	"  }"\` |
|       - |   46 | ``	"  /* php's `?resource $context` refusal, spelled here for the same reason the"\`` |
|       - |   47 | `	"     NUL check above is: forwarding an invalid one to opendir() would name"\` |
|       - |   48 | `	"     opendir() in a message php raises against scandir(). */"\` |
|       - |   49 | `	"  if( $context !== null ){"\` |
|       - |   50 | `	"    if( !is_resource($context) ){"\` |
|       - |   51 | `	"      throw new TypeError('scandir(): Argument #3 ($context) must be of type resource or null, '"\` |
|       - |   52 | `	"        . get_debug_type($context) . ' given');"\` |
|       - |   53 | `	"    }"\` |
|       - |   54 | `	"    if( get_resource_type($context) !== 'stream-context' ){"\` |
|       - |   55 | `	"      throw new TypeError('scandir(): supplied resource is not a valid Stream-Context resource');"\` |
|       - |   56 | `	"    }"\` |
|       - |   57 | `	"  }"\` |
|       - |   58 | `	"  $aDir = array();"\` |
|       - |   59 | `	"  /* php hands the context straight to the open it performs; dropping it here"\` |
|       - |   60 | `	"     was the same unread argument every C opener had. */"\` |
|       - |   61 | `	"  $pHandle = opendir($directory, $context);"\` |
|       - |   62 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|       - |   63 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|       - |   64 | `	"      $aDir[] = $pEntry;"\` |
|       - |   65 | `	"   }"\` |
|       - |   66 | `	"  closedir($pHandle);"\` |
|       - |   67 | `	"  /* php's rule is a two-way split, not a three-value enum: SORT_NONE leaves the"\` |
|       - |   68 | `	"     order alone and EVERY other value sorts -- ascending only for the exact"\` |
|       - |   69 | `	"     SORT_ASCENDING, descending otherwise. PHL left an unknown value UNSORTED,"\` |
|       - |   70 | `	"     which reads as SORT_NONE."\` |
|       - |   71 | `	"     The comparison is php_stream_dirent_alphasort's strcoll(), which in the C"\` |
|       - |   72 | `	"     locale php runs in is a BYTE compare -- so SORT_STRING, not the default"\` |
|       - |   73 | `	"     SORT_REGULAR. Sorting by VALUE ordered numeric names numerically, which is"\` |
|       - |   74 | ``	"     a different listing: `9` came before `10`, and `00` before `0`. */"\`` |
|       - |   75 | `	"  if( $sorting_order != SCANDIR_SORT_NONE ){"\` |
|       - |   76 | `	"      if( $sorting_order == SCANDIR_SORT_ASCENDING ){ sort($aDir,SORT_STRING); }"\` |
|       - |   77 | `	"      else { rsort($aDir,SORT_STRING); }"\` |
|       - |   78 | `	"  }"\` |
|       - |   79 | `	"  return $aDir;"\` |
|       - |   80 | `	"}"\` |
|       - |   81 | `	"function glob(string $pattern,int $flags = 0): array\|false {"\` |
|       - |   82 | `	"/* php's glob() is the C library's glob(3) over the RAW pattern -- it is the one"\` |
|       - |   83 | `	"   directory reader php does not route through a stream wrapper, so a pattern"\` |
|       - |   84 | `	"   carrying a scheme names a directory that does not exist and matches nothing."\` |
|       - |   85 | `	"   PHL globbed through the wrapper lookup opendir() uses, so glob('file://'.'/tmp/*')"\` |
|       - |   86 | `	"   listed /tmp where php answers [], and a userland wrapper's own listing would"\` |
|       - |   87 | `	"   have answered here too once its directory door opened. */"\` |
|       - |   88 | `	"if( preg_match('#^[A-Za-z][A-Za-z0-9+.\\\\-]*://#', $pattern) ){ return array(); }"\` |
|       - |   89 | `	"/* php's Z_PARAM_PATH refusal (see scandir above). It precedes the flag check:"\` |
|       - |   90 | `	"   php's ZPP runs before the function body. Without it the NUL was simply the end"\` |
|       - |   91 | `	"   of the pattern and glob() answered for the truncated one. */"\` |
|       - |   92 | `	"if( strpos($pattern, chr(0)) !== false ){"\` |
|       - |   93 | `	"  throw new ValueError('glob(): Argument #1 ($pattern) must not contain any null bytes');"\` |
|       - |   94 | `	"}"\` |
|       - |   95 | `	"/* php rejects a mask holding any bit outside GLOB_AVAILABLE_FLAGS with a warning"\` |
|       - |   96 | `	"   and FALSE. PHL accepted anything and just tested the bits it knew, so a stale"\` |
|       - |   97 | `	"   script passing the OLD PHL glob values (1/2/4/...) silently got a plain glob. */"\` |
|       - |   98 | `	"if( $flags & ~GLOB_AVAILABLE_FLAGS ){"\` |
|       - |   99 | `	"  trigger_error('glob(): At least one of the passed flags is invalid or not supported on this platform', E_USER_WARNING);"\` |
|       - |  100 | `	"  return FALSE;"\` |
|       - |  101 | `	"}"\` |
|       - |  102 | `	"/* GLOB_BRACE: expand the FIRST top-level {a,b,...} group and glob each"\` |
|       - |  103 | `	"   alternative IN ORDER, concatenating the answers (each sub-glob sorts its"\` |
|       - |  104 | `	"   own results; php never re-sorts across alternatives). Nested groups are"\` |
|       - |  105 | `	"   handled by the recursion, and GLOB_NOCHECK applies per EXPANDED pattern,"\` |
|       - |  106 | `	"   which is php's answer too. The flag used to be accepted and IGNORED, so"\` |
|       - |  107 | `	"   any braced pattern answered [] in silence. */"\` |
|       - |  108 | `	"if( $flags & GLOB_BRACE ){"\` |
|       - |  109 | `	"  $nLen = strlen($pattern); $iOpen = -1; $iClose = -1; $iDepth = 0;"\` |
|       - |  110 | `	"  for( $i = 0 ; $i < $nLen ; $i++ ){"\` |
|       - |  111 | `	"    $ch = $pattern[$i];"\` |
|       - |  112 | `	"    if( $ch === '{' ){ if( $iDepth === 0 ){ $iOpen = $i; } $iDepth++; }"\` |
|       - |  113 | `	"    else if( $ch === '}' && $iDepth > 0 ){ $iDepth--; if( $iDepth === 0 ){ $iClose = $i; break; } }"\` |
|       - |  114 | `	"  }"\` |
|       - |  115 | `	"  if( $iOpen >= 0 && $iClose > $iOpen ){"\` |
|       - |  116 | `	"    $zHead = substr($pattern,0,$iOpen);"\` |
|       - |  117 | `	"    $zBody = (string)substr($pattern,$iOpen+1,$iClose-$iOpen-1);"\` |
|       - |  118 | `	"    $zTail = (string)substr($pattern,$iClose+1);"\` |
|       - |  119 | `	"    $aAlt = array(); $zCur = ''; $iDepth = 0;"\` |
|       - |  120 | `	"    for( $i = 0 ; $i < strlen($zBody) ; $i++ ){"\` |
|       - |  121 | `	"      $ch = $zBody[$i];"\` |
|       - |  122 | `	"      if( $ch === '{' ){ $iDepth++; }"\` |
|       - |  123 | `	"      else if( $ch === '}' ){ $iDepth--; }"\` |
|       - |  124 | `	"      if( $ch === ',' && $iDepth === 0 ){ $aAlt[] = $zCur; $zCur = ''; continue; }"\` |
|       - |  125 | `	"      $zCur .= $ch;"\` |
|       - |  126 | `	"    }"\` |
|       - |  127 | `	"    $aAlt[] = $zCur;"\` |
|       - |  128 | `	"    $pArray = array();"\` |
|       - |  129 | `	"    foreach( $aAlt as $zAlt ){"\` |
|       - |  130 | `	"      $aSub = glob($zHead . $zAlt . $zTail,$flags);"\` |
|       - |  131 | `	"      if( $aSub !== false ){ foreach( $aSub as $zHit ){ $pArray[] = $zHit; } }"\` |
|       - |  132 | `	"    }"\` |
|       - |  133 | `	"    return $pArray;"\` |
|       - |  134 | `	"  }"\` |
|       - |  135 | `	"}"\` |
|       - |  136 | `	"/* A pattern that ENDS in a slash names DIRECTORIES, and php keeps the slash:"\` |
|       - |  137 | `	"   glob('d/') is ['d/'] and glob('d/*' . '/') is ['d/a/','d/b/']. Answer the base"\` |
|       - |  138 | `	"   without it, as directories, and put it back -- ONE slash at a time, so a"\` |
|       - |  139 | `	"   pattern ending in two keeps both. */"\` |
|       - |  140 | `	"if( substr($pattern,-1) === '/' ){"\` |
|       - |  141 | `	"  $zBase = substr($pattern,0,-1);"\` |
|       - |  142 | `	"  if( $zBase === '' ){"\` |
|       - |  143 | `	"    /* the pattern was '/' itself */"\` |
|       - |  144 | `	"    $pArray = is_dir('/') ? array('/') : array();"\` |
|       - |  145 | `	"  }else{"\` |
|       - |  146 | `	"    $pArray = array();"\` |
|       - |  147 | `	"    foreach( glob($zBase,($flags & ~(GLOB_MARK\|GLOB_NOCHECK)) \| GLOB_ONLYDIR) as $zHit ){"\` |
|       - |  148 | `	"      $pArray[] = $zHit . '/';"\` |
|       - |  149 | `	"    }"\` |
|       - |  150 | ``	"    /* php sorts the names it ANSWERS, slash included, so `a/../` comes before"\`` |
|       - |  151 | ``	"       `a/./` -- sorting the bases and appending afterwards has them the other"\`` |
|       - |  152 | `	"       way round. */"\` |
|       - |  153 | `	"    if( ($flags & GLOB_NOSORT) == 0 ){ sort($pArray,SORT_STRING); }"\` |
|       - |  154 | `	"  }"\` |
|       - |  155 | `	"  if( ($flags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){ $pArray[] = $pattern; }"\` |
|       - |  156 | `	"  return $pArray;"\` |
|       - |  157 | `	"}"\` |
|       - |  158 | `	"/* A wildcard in the DIRECTORY part is matched LEVEL BY LEVEL, which is what"\` |
|       - |  159 | `	"   glob(3) does: list the directories that part names, then glob the last"\` |
|       - |  160 | `	"   component inside each. Reading only the last component -- all this used to"\` |
|       - |  161 | ``	"   do -- answered [] for `src/*' . '/*.php', the everyday two-level spelling,"\`` |
|       - |  162 | `	"   and for every deeper one. */"\` |
|       - |  163 | `	"$slash = strrpos($pattern,'/');"\` |
|       - |  164 | `	"if( $slash !== false ){"\` |
|       - |  165 | `	"  $zHead = substr($pattern,0,$slash);"\` |
|       - |  166 | `	"  if( $zHead !== '' && strcspn($zHead,'*?[') != strlen($zHead) ){"\` |
|       - |  167 | `	"    $pArray = array();"\` |
|       - |  168 | `	"    foreach( glob($zHead . '/') as $zDirHit ){"\` |
|       - |  169 | `	"      foreach( glob($zDirHit . substr($pattern,$slash+1),$flags & ~GLOB_NOCHECK) as $zHit ){"\` |
|       - |  170 | `	"        $pArray[] = $zHit;"\` |
|       - |  171 | `	"      }"\` |
|       - |  172 | `	"    }"\` |
|       - |  173 | `	"    if( ($flags & GLOB_NOSORT) == 0 ){ sort($pArray,SORT_STRING); }"\` |
|       - |  174 | `	"    if( ($flags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){ $pArray[] = $pattern; }"\` |
|       - |  175 | `	"    return $pArray;"\` |
|       - |  176 | `	"  }"\` |
|       - |  177 | `	"}"\` |
|       - |  178 | `	"/* php keeps the literal directory portion of the pattern in every result;"\` |
|       - |  179 | `	"   split off everything up to and including the last '/' as the prefix. */"\` |
|       - |  180 | `	"$slash = strrpos($pattern,'/');"\` |
|       - |  181 | `	"if( $slash === false ){ $zDir = '.'; $prefix = ''; $pat = $pattern; }"\` |
|       - |  182 | `	"else { $zDir = substr($pattern,0,$slash); if( $zDir === '' ){ $zDir = '/'; } $prefix = substr($pattern,0,$slash+1); $pat = substr($pattern,$slash+1); }"\` |
|       - |  183 | `	"$pArray = array(); /* Empty array */"\` |
|       - |  184 | `	"/* php answers [] in SILENCE for a directory that cannot be opened — a"\` |
|       - |  185 | `	"   nonexistent path is simply zero matches (GLOB_ERR included; that flag is"\` |
|       - |  186 | `	"   about errors during the walk, not about the path). PHL used to let"\` |
|       - |  187 | `	"   opendir() warn and answered FALSE. */"\` |
|       - |  188 | `	"$pHandle = @opendir($zDir);"\` |
|       - |  189 | `	"if( $pHandle != FALSE ){"\` |
|       - |  190 | `	"/* Loop throw available entries */"\` |
|       - |  191 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|       - |  192 | `	" /* php's glob() never matches a leading-dot entry (incl. '.' and '..') unless"\` |
|       - |  193 | `	"    the pattern itself starts with a dot */"\` |
|       - |  194 | `	"	if( strlen($pEntry) > 0 && $pEntry[0] === '.' && (strlen($pat) < 1 \|\| $pat[0] !== '.') ){ continue; }"\` |
|       - |  195 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|       - |  196 | `	"	$rc = strglob($pat,$pEntry);"\` |
|       - |  197 | `	"	if( $rc ){"\` |
|       - |  198 | `	"	   $zFull = $prefix . $pEntry;"\` |
|       - |  199 | `	"	   if( is_dir($zDir . '/' . $pEntry) ){"\` |
|       - |  200 | `	"	      if( $flags & GLOB_MARK ){"\` |
|       - |  201 | `	"		     /* Adds a slash to each directory returned */"\` |
|       - |  202 | `	"			 $zFull .= DIRECTORY_SEPARATOR;"\` |
|       - |  203 | `	"		  }"\` |
|       - |  204 | `	"	   }else if( $flags & GLOB_ONLYDIR ){"\` |
|       - |  205 | `	"	     /* Not a directory,ignore */"\` |
|       - |  206 | `	"		 continue;"\` |
|       - |  207 | `	"	   }"\` |
|       - |  208 | `	"	   /* Add the entry (with its literal directory prefix, php-style) */"\` |
|       - |  209 | `	"	   $pArray[] = $zFull;"\` |
|       - |  210 | `	"	}"\` |
|       - |  211 | `	" }"\` |
|       - |  212 | `	"/* Close the handle */"\` |
|       - |  213 | `	"closedir($pHandle);"\` |
|       - |  214 | `	"}"\` |
|       - |  215 | `	"if( ($flags & GLOB_NOSORT) == 0 ){"\` |
|       - |  216 | `	"  /* glob(3) sorts with strcoll(), a BYTE compare in the C locale php runs in,"\` |
|       - |  217 | `	"     and it sorts the whole ANSWER rather than each directory it walked. The"\` |
|       - |  218 | `	"     default SORT_REGULAR compared numeric names as NUMBERS, so a directory of"\` |
|       - |  219 | ``	"     `1.jpg`..`10.jpg` came back in a different order than php lists it. */"\`` |
|       - |  220 | `	"  sort($pArray,SORT_STRING);"\` |
|       - |  221 | `	"}"\` |
|       - |  222 | `	"if( ($flags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|       - |  223 | `	"  /* Return the search pattern if no files matching were found */"\` |
|       - |  224 | `	"  $pArray[] = $pattern;"\` |
|       - |  225 | `	"}"\` |
|       - |  226 | `	"/* Return the created array */"\` |
|       - |  227 | `	"return $pArray;"\` |
|       - |  228 | `   "}"\` |
|       - |  229 | `   "/* Creates a temporary file */"\` |
|       - |  230 | `   "function tmpfile(){"\` |
|       - |  231 | `   "  /* Extract the temp directory */"\` |
|       - |  232 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|       - |  233 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|       - |  234 | `   "    /* Use the current dir */"\` |
|       - |  235 | `   "    $zTempDir = '.';"\` |
|       - |  236 | `   "  }"\` |
|       - |  237 | `   "  /* Create the file */"\` |
|       - |  238 | `   "  $zPath = $zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12);"\` |
|       - |  239 | `   "  /* php CREATES the file and then opens it r+b, which is the mode"\` |
|       - |  240 | `   "   * stream_get_meta_data() reports back for it. */"\` |
|       - |  241 | `   "  fclose(fopen($zPath,'w'));"\` |
|       - |  242 | `   "  $pHandle = fopen($zPath,'r+b');"\` |
|       - |  243 | `   "  return $pHandle;"\` |
|       - |  244 | `   "}"\` |
|       - |  245 | `   /* A builtin written here is INTERNAL to php, and php declares every one of` |
|       - |  246 | `    * these parameters. The declaration is not decoration: it is the ZPP screen,` |
|       - |  247 | `    * so an untyped $num CAST what php refuses -- is_nan('abc') answered false` |
|       - |  248 | `    * where php raises a TypeError, and each of the manual casts these bodies` |
|       - |  249 | `    * opened with was hiding exactly that. The return types are php's too; they` |
|       - |  250 | `    * are what ReflectionFunction prints. */\` |
|       - |  251 | `   "function is_nan(float $num): bool { return $num != $num; }"\` |
|       - |  252 | `   "function is_infinite(float $num): bool { return $num == INF \|\| $num == -INF; }"\` |
|       - |  253 | `   "function is_finite(float $num): bool { return !is_nan($num) && !is_infinite($num); }"\` |
|       - |  254 | `   "/* Inverse of bin2hex() */"\` |
|       - |  255 | `   "function hex2bin(string $string): string\|false {"\` |
|       - |  256 | `   "  $len = strlen($string);"\` |
|       - |  257 | `   "  if( $len % 2 !== 0 ){"\` |
|       - |  258 | `   "    trigger_error('hex2bin(): Hexadecimal input string must have an even length', E_USER_WARNING);"\` |
|       - |  259 | `   "    return false;"\` |
|       - |  260 | `   "  }"\` |
|       - |  261 | `   "  $out = '';"\` |
|       - |  262 | `   "  for( $i = 0 ; $i < $len ; $i += 2 ){"\` |
|       - |  263 | `   "    $pair = substr($string, $i, 2);"\` |
|       - |  264 | `   "    if( !ctype_xdigit($pair) ){"\` |
|       - |  265 | `   "      trigger_error('hex2bin(): Input string must be hexadecimal string', E_USER_WARNING);"\` |
|       - |  266 | `   "      return false;"\` |
|       - |  267 | `   "    }"\` |
|       - |  268 | `   "    $out = $out . chr(hexdec($pair));"\` |
|       - |  269 | `   "  }"\` |
|       - |  270 | `   "  return $out;"\` |
|       - |  271 | `   "}"\` |
|       - |  272 | ``   "/* Division that never throws: INF/-INF/NAN like php. The two `float`"\`` |
|       - |  273 | `   " * declarations are php's own: they are what refuses a non-numeric string"\` |
|       - |  274 | `   " * (an untyped $num1 cast to 0.0 and DIVIDED, so fdiv('abc',2) answered"\` |
|       - |  275 | `   " * float(0)), and what ReflectionFunction prints. */"\` |
|       - |  276 | `   "function fdiv(float $num1, float $num2): float {"\` |
|       - |  277 | `   "  if( $num2 == 0.0 ){"\` |
|       - |  278 | `   "    if( $num1 == 0.0 \|\| is_nan($num1) ){ return NAN; }"\` |
|       - |  279 | `   "    return $num1 > 0 ? INF : -INF;"\` |
|       - |  280 | `   "  }"\` |
|       - |  281 | `   "  return $num1 / $num2;"\` |
|       - |  282 | `   "}"\` |
|       - |  283 | `   "function checkdate(int $month, int $day, int $year): bool {"\` |
|       - |  284 | `   "  if( $month < 1 \|\| $month > 12 \|\| $year < 1 \|\| $year > 32767 \|\| $day < 1 ){ return false; }"\` |
|       - |  285 | `   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\` |
|       - |  286 | `   "  $max = $days[$month - 1];"\` |
|       - |  287 | `   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) \|\| ($year % 400 === 0)) ){"\` |
|       - |  288 | `   "    $max = 29;"\` |
|       - |  289 | `   "  }"\` |
|       - |  290 | `   "  return $day <= $max;"\` |
|       - |  291 | `   "}"\` |
|       - |  292 | `   "function is_iterable(mixed $value): bool { return is_array($value) \|\| ($value instanceof Traversable); }"\` |
|       - |  293 | `   "function is_countable(mixed $value): bool { return is_array($value) \|\| ($value instanceof Countable); }"\` |
|       - |  294 | `   "function doubleval(mixed $value): float { return (float)$value; }"\` |
|       - |  295 | `   "function array_count_values(array $array): array {"\` |
|       - |  296 | `   "  $out = array();"\` |
|       - |  297 | `   "  foreach( $array as $v ){"\` |
|       - |  298 | `   "    if( !is_int($v) && !is_string($v) ){"\` |
|       - |  299 | `   "      trigger_error('array_count_values(): Can only count string and integer values, entry skipped', E_USER_WARNING);"\` |
|       - |  300 | `   "      continue;"\` |
|       - |  301 | `   "    }"\` |
|       - |  302 | `   "    if( isset($out[$v]) ){ $out[$v] = $out[$v] + 1; } else { $out[$v] = 1; }"\` |
|       - |  303 | `   "  }"\` |
|       - |  304 | `   "  return $out;"\` |
|       - |  305 | `   "}"\` |
|       - |  306 | `   "function array_change_key_case(array $array, int $case = CASE_LOWER): array {"\` |
|       - |  307 | `   "  $out = array();"\` |
|       - |  308 | `   "  foreach( $array as $k => $v ){"\` |
|       - |  309 | `   "    if( is_string($k) ){ $k = ($case == CASE_UPPER) ? strtoupper($k) : strtolower($k); }"\` |
|       - |  310 | `   "    $out[$k] = $v;"\` |
|       - |  311 | `   "  }"\` |
|       - |  312 | `   "  return $out;"\` |
|       - |  313 | `   "}"\` |
|       - |  314 | `   "function array_replace_recursive(array $array, array ...$replacements): array {"\` |
|       - |  315 | `   "  foreach( $replacements as $o ){"\` |
|       - |  316 | `   "    foreach( $o as $k => $v ){"\` |
|       - |  317 | `   "      if( is_array($v) && isset($array[$k]) && is_array($array[$k]) ){"\` |
|       - |  318 | `   "        $array[$k] = array_replace_recursive($array[$k], $v);"\` |
|       - |  319 | `   "      }else{"\` |
|       - |  320 | `   "        $array[$k] = $v;"\` |
|       - |  321 | `   "      }"\` |
|       - |  322 | `   "    }"\` |
|       - |  323 | `   "  }"\` |
|       - |  324 | `   "  return $array;"\` |
|       - |  325 | `   "}"\` |
|       - |  326 | `   /* class_parents/class_implements/class_uses moved to C (vm_builtin_class.c):` |
|       - |  327 | `    * as prelude wrappers they gated on class_exists(), so an interface, a trait` |
|       - |  328 | `    * and an enum all answered FALSE where php answers a list; class_uses could` |
|       - |  329 | `    * not reach the trait table at all and returned the empty set for every class;` |
|       - |  330 | `    * and the E_WARNING php raises for a name nothing declares cannot be raised` |
|       - |  331 | `    * from here at php's severity or against the CALLER's line. */\` |
|       - |  332 | `   "function ip2long(string $ip): int\|false {"\` |
|       - |  333 | `   "  $p = explode('.', $ip);"\` |
|       - |  334 | `   "  if( count($p) !== 4 ){ return false; }"\` |
|       - |  335 | `   "  $n = 0;"\` |
|       - |  336 | `   "  foreach( $p as $o ){"\` |
|       - |  337 | `   "    if( !ctype_digit($o) \|\| (int)$o < 0 \|\| (int)$o > 255 ){ return false; }"\` |
|       - |  338 | `   "    $n = $n * 256 + (int)$o;"\` |
|       - |  339 | `   "  }"\` |
|       - |  340 | `   "  return $n;"\` |
|       - |  341 | `   "}"\` |
|       - |  342 | `   "function long2ip(int $ip): string {"\` |
|       - |  343 | `   "  return (($ip >> 24) & 255) . '.' . (($ip >> 16) & 255) . '.' . (($ip >> 8) & 255) . '.' . ($ip & 255);"\` |
|       - |  344 | `   "}"\` |
|       - |  345 | `   "/* php 8.3 str_increment(): Perl-style alphanumeric increment. */"\` |
|       - |  346 | `   "function str_increment(string $string): string {"\` |
|       - |  347 | `   "  if( $string === '' ){ throw new ValueError('str_increment(): Argument #1 ($string) must not be empty'); }"\` |
|       - |  348 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_increment(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|       - |  349 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|       - |  350 | `   "    $c = $string[$i];"\` |
|       - |  351 | `   "    if( $c === 'z' ){ $string[$i] = 'a'; }"\` |
|       - |  352 | `   "    elseif( $c === 'Z' ){ $string[$i] = 'A'; }"\` |
|       - |  353 | `   "    elseif( $c === '9' ){ $string[$i] = '0'; }"\` |
|       - |  354 | `   "    else { $string[$i] = chr(ord($c) + 1); return $string; }"\` |
|       - |  355 | `   "  }"\` |
|       - |  356 | `   "  $first = $string[0];"\` |
|       - |  357 | `   "  if( $first === '0' ){ return '1' . $string; }"\` |
|       - |  358 | `   "  if( $first === 'a' ){ return 'a' . $string; }"\` |
|       - |  359 | `   "  return 'A' . $string;"\` |
|       - |  360 | `   "}"\` |
|       - |  361 | `   "/* php 8.3 str_decrement(): inverse of str_increment(); throws out of range"\` |
|       - |  362 | `   " * at the bottom of the counting sequence. */"\` |
|       - |  363 | `   "function str_decrement(string $string): string {"\` |
|       - |  364 | `   "  if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) must not be empty'); }"\` |
|       - |  365 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_decrement(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|       - |  366 | `   "  $orig = $string;"\` |
|       - |  367 | `   "  $borrowed = false;"\` |
|       - |  368 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|       - |  369 | `   "    $c = $string[$i];"\` |
|       - |  370 | `   "    if( $c === 'a' ){ $string[$i] = 'z'; }"\` |
|       - |  371 | `   "    elseif( $c === 'A' ){ $string[$i] = 'Z'; }"\` |
|       - |  372 | `   "    elseif( $c === '0' ){ $string[$i] = '9'; }"\` |
|       - |  373 | `   "    else { $string[$i] = chr(ord($c) - 1); $borrowed = false; break; }"\` |
|       - |  374 | `   "    if( $i === 0 ){ $borrowed = true; }"\` |
|       - |  375 | `   "  }"\` |
|       - |  376 | `   "  if( $borrowed ){"\` |
|       - |  377 | `   "    if( $string[0] === '9' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|       - |  378 | `   "    $string = substr($string, 1);"\` |
|       - |  379 | `   "    if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|       - |  380 | `   "  } elseif( strlen($string) > 1 && $string[0] === '0' ){"\` |
|       - |  381 | `   "    $string = substr($string, 1);"\` |
|       - |  382 | `   "  }"\` |
|       - |  383 | `   "  return $string;"\` |
|       - |  384 | `   "}"\` |
|       - |  385 | `   /* fileperms/fileowner/filegroup/fileinode moved to C (vfs.c, VfsStatField):` |
|       - |  386 | `    * as prelude wrappers over stat() three of them said nothing on a failed stat` |
|       - |  387 | `    * and the fourth raised trigger_error, whose errno is E_USER_WARNING's 512 and` |
|       - |  388 | `    * whose line is this chunk's rather than the caller's. */\` |
|       - |  389 | `   "/* PH7 keeps no stat cache, so this is a no-op like php on a clean cache. */"\` |
|       - |  390 | `   "function clearstatcache(bool $clear_realpath_cache = false, string $filename = ''): void {}"\` |
|       - |  391 | `   /* mb_ucfirst/mb_lcfirst moved to C (builtin_mb.c): as prelude wrappers they` |
|       - |  392 | `    * dropped $encoding, UPPER-cased where php title-cases ('ß' -> 'SS' for php's` |
|       - |  393 | `    * 'Ss') and lowered a leading Σ with nothing after it, which is php's FINAL` |
|       - |  394 | `    * sigma and not what a first character gets. */\` |
|       - |  395 | `   "/* Creates a temporary file and returns its name */"\` |
|       - |  396 | `   "function tempnam(string $directory,string $prefix): string\|false"\` |
|       - |  397 | `   "{"\` |
|       - |  398 | `   "   /* php's Z_PARAM_PATH refusal on BOTH parameters (see scandir above); the prefix"\` |
|       - |  399 | `   "    * is a path fragment there too, and PHL used to build a filename with the NUL"\` |
|       - |  400 | `   "    * still in it. */"\` |
|       - |  401 | `   "   if( strpos($directory, chr(0)) !== false ){"\` |
|       - |  402 | `   "     throw new ValueError('tempnam(): Argument #1 ($directory) must not contain any null bytes');"\` |
|       - |  403 | `   "   }"\` |
|       - |  404 | `   "   if( strpos($prefix, chr(0)) !== false ){"\` |
|       - |  405 | `   "     throw new ValueError('tempnam(): Argument #2 ($prefix) must not contain any null bytes');"\` |
|       - |  406 | `   "   }"\` |
|       - |  407 | `   "   /* php falls back to the system temporary directory when the one it was"\` |
|       - |  408 | `   "    * given cannot HOLD the file, and says so -- except for the empty"\` |
|       - |  409 | ``   "    * directory, which it reads as `use the temp dir` and answers silently."\`` |
|       - |  410 | `   "    * PHL took '' literally and spent 64 tries failing at the filesystem"\` |
|       - |  411 | `   "    * ROOT. Whether a directory can hold it is settled by TRYING, not by"\` |
|       - |  412 | `   "    * asking is_writable(): the two disagree on Windows. */"\` |
|       - |  413 | `   "   $zTmp = rtrim(sys_get_temp_dir(), DIRECTORY_SEPARATOR);"\` |
|       - |  414 | `   "   $zDir = $directory === '' ? $zTmp : rtrim($directory, DIRECTORY_SEPARATOR);"\` |
|       - |  415 | `   "   if( is_dir($zDir) && is_writable($zDir) ){"\` |
|       - |  416 | `   "     $zOut = __tempnam_in($zDir, $prefix);"\` |
|       - |  417 | `   "     if( $zOut !== false ){ return $zOut; }"\` |
|       - |  418 | `   "   }"\` |
|       - |  419 | `   "   if( $zDir === $zTmp ){ return false; }"\` |
|       - |  420 | `   "   trigger_error(\"tempnam(): file created in the system's temporary directory\", E_USER_NOTICE);"\` |
|       - |  421 | `   "   return __tempnam_in($zTmp, $prefix);"\` |
|       - |  422 | `   "}"\` |
|       - |  423 | `   "function __tempnam_in(string $zDir, string $prefix)"\` |
|       - |  424 | `   "{"\` |
|       - |  425 | `   "   /* php CREATES the file (empty, mode 0600) and guarantees the name is"\` |
|       - |  426 | `   "    * unique -- returning a bare name left the caller with a path that does"\` |
|       - |  427 | `   "    * not exist, so file_exists() was false and unlink() failed on it. */"\` |
|       - |  428 | `   "   for( $i = 0 ; $i < 64 ; ++$i ){"\` |
|       - |  429 | `   "     $zPath = $zDir.DIRECTORY_SEPARATOR.$prefix.rand_str(12);"\` |
|       - |  430 | `   "     if( file_exists($zPath) ){ continue; }"\` |
|       - |  431 | `   "     $pHandle = @fopen($zPath,'x');"\` |
|       - |  432 | `   "     if( $pHandle === false ){ return false; }"\` |
|       - |  433 | `   "     fclose($pHandle);"\` |
|       - |  434 | `   "     @chmod($zPath, 0600);"\` |
|       - |  435 | `   "     return $zPath;"\` |
|       - |  436 | `   "   }"\` |
|       - |  437 | `   "   return false;"\` |
|       - |  438 | `   "}"\` |
|       - |  439 | `	/* fileowner/filegroup/fileinode: see the note beside fileperms above. */\` |
|       - |  440 | `	""` |
|       - |  441 |  |
|       - |  442 | `/*` |
|       - |  443 | ` * ---------------------------------------------------------------------------` |
|       - |  444 | ` * The Exception / Error family, declared from C.` |
|       - |  445 | ` *` |
|       - |  446 | ` * php's two roots are one implementation twice over (its stub says` |
|       - |  447 | `` * `@implementation-alias Exception::__construct` for every one of Error's`` |
|       - |  448 | ` * methods), so the bodies below are shared by both spec tables and the` |
|       - |  449 | ` * ~20 subclasses are declaration-only rows.` |
|       - |  450 | ` *` |
|       - |  451 | `` * php's seven slots, in php's own declaration order. `string` is php's cache of`` |
|       - |  452 | ` * the __toString rendering -- unused by the engine but PRESENT on every` |
|       - |  453 | ` * presentation surface, which is why it is declared here rather than skipped:` |
|       - |  454 | ` * var_dump/print_r/(array)/serialize all show it, and PHL was one property short` |
|       - |  455 | ` * of php on every exception ever printed.` |
|       - |  456 | ` * ---------------------------------------------------------------------------` |
|       - |  457 | ` */` |
|       - |  458 | `#define EXC_MESSAGE  "message"` |
|       - |  459 | `#define EXC_STRING   "string"` |
|       - |  460 | `#define EXC_CODE     "code"` |
|       - |  461 | `#define EXC_FILE     "file"` |
|       - |  462 | `#define EXC_LINE     "line"` |
|       - |  463 | `#define EXC_TRACE    "trace"` |
|       - |  464 | `#define EXC_PREVIOUS "previous"` |
|       - |  465 | `#define EXC_SEVERITY "severity"` |
|       - |  466 | `/*` |
|       - |  467 | ` * Answer a declared slot the way php's getter does. Three of the seven CONVERT` |
|       - |  468 | ` * rather than copy — getMessage()/getFile() answer a string and getLine() an int,` |
|       - |  469 | ` * whatever the slot holds — and that shows twice: a subclass assigning` |
|       - |  470 | `` * `$this->message = 5` reads back "5", and a slot __wakeup has DROPPED reads as`` |
|       - |  471 | ` * "" rather than null. The other four are verbatim copies (getCode() of that same` |
|       - |  472 | ` * subclass really is the int).` |
|       - |  473 | ` */` |
|       - |  474 | `#define EXC_READ_RAW 0` |
|       - |  475 | `#define EXC_READ_STR 1` |
|       - |  476 | `#define EXC_READ_INT 2` |
|   17937 |  477 | `static int VmExcReadSlot(ph7_context *pCtx,const char *zSlot,int iAs)` |
|       5 |  478 | `{` |
|   17942 |  479 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   17942 |  480 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,zSlot) : 0;` |
|       - |  481 | `	ph7_value sTmp;` |
|   17942 |  482 | `	if( iAs == EXC_READ_RAW ){` |
|     548 |  483 | `		if( pVal ){` |
|     548 |  484 | `			ph7_result_value(pCtx,pVal);` |
|     276 |  485 | `		}else{` |
|     ! 0 |  486 | `			ph7_result_null(pCtx);` |
|       - |  487 | `		}` |
|     548 |  488 | `		return PH7_OK;` |
|       - |  489 | `	}` |
|       - |  490 | `	/* Through a COPY: converting the slot would rewrite the exception's state. */` |
|   17398 |  491 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|   17398 |  492 | `	if( pVal ){` |
|   17398 |  493 | `		PH7_MemObjStore(pVal,&sTmp);` |
|    8669 |  494 | `	}` |
|   17398 |  495 | `	if( iAs == EXC_READ_INT ){` |
|     695 |  496 | `		PH7_MemObjToInteger(&sTmp);` |
|     350 |  497 | `	}else{` |
|   16708 |  498 | `		PH7_MemObjToString(&sTmp);` |
|       - |  499 | `	}` |
|   17398 |  500 | `	ph7_result_value(pCtx,&sTmp);` |
|   17398 |  501 | `	PH7_MemObjRelease(&sTmp);` |
|   17398 |  502 | `	return PH7_OK;` |
|    8946 |  503 | `}` |
|   16049 |  504 | `static int vm_builtin_Exception_getMessage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  505 | `{` |
|    7997 |  506 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   16054 |  507 | `	return VmExcReadSlot(pCtx,EXC_MESSAGE,EXC_READ_STR);` |
|       5 |  508 | `}` |
|     494 |  509 | `static int vm_builtin_Exception_getCode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  510 | `{` |
|     247 |  511 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     498 |  512 | `	return VmExcReadSlot(pCtx,EXC_CODE,EXC_READ_RAW);` |
|       4 |  513 | `}` |
|     654 |  514 | `static int vm_builtin_Exception_getFile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  515 | `{` |
|     327 |  516 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     659 |  517 | `	return VmExcReadSlot(pCtx,EXC_FILE,EXC_READ_STR);` |
|       5 |  518 | `}` |
|     690 |  519 | `static int vm_builtin_Exception_getLine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  520 | `{` |
|     345 |  521 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     695 |  522 | `	return VmExcReadSlot(pCtx,EXC_LINE,EXC_READ_INT);` |
|       5 |  523 | `}` |
|      24 |  524 | `static int vm_builtin_Exception_getTrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  525 | `{` |
|      12 |  526 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      26 |  527 | `	return VmExcReadSlot(pCtx,EXC_TRACE,EXC_READ_RAW);` |
|       2 |  528 | `}` |
|      22 |  529 | `static int vm_builtin_Exception_getPrevious(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  530 | `{` |
|      11 |  531 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      24 |  532 | `	return VmExcReadSlot(pCtx,EXC_PREVIOUS,EXC_READ_RAW);` |
|       2 |  533 | `}` |
|       4 |  534 | `static int vm_builtin_ErrorException_getSeverity(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  535 | `{` |
|       2 |  536 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       5 |  537 | `	return VmExcReadSlot(pCtx,EXC_SEVERITY,EXC_READ_RAW);` |
|       1 |  538 | `}` |
|       - |  539 | `/*` |
|       - |  540 | ` * php's zend_update_exception_properties: each of the three is written only when` |
|       - |  541 | ``  * the caller actually supplied it — a message when the argument was PASSED (`""` `` |
|       - |  542 | ` * included), a code when it is NON-ZERO, a previous when it is an object. That is` |
|       - |  543 | ` * not the same as writing the defaults: a subclass may redeclare` |
|       - |  544 | `` * `protected $message = 'default'`, and php keeps it for `new Sub()`.`` |
|       - |  545 | ` */` |
| 1465611 |  546 | `static void VmExcInitProps(ph7_context *pCtx,ph7_class_instance *pThis,int nArg,` |
|       - |  547 | `	ph7_value **apArg,int iPrev)` |
|       5 |  548 | `{` |
| 1465616 |  549 | `	if( nArg > 0 ){` |
| 1465514 |  550 | `		int nMsg = 0;` |
| 1465514 |  551 | `		const char *zMsg = ph7_value_to_string(apArg[0],&nMsg);` |
| 1465514 |  552 | `		PH7_NativeSetAttrStr(pCtx->pVm,pThis,EXC_MESSAGE,zMsg,nMsg);` |
|  732727 |  553 | `	}` |
| 1465616 |  554 | `	if( nArg > 1 ){` |
|       - |  555 | `		ph7_value sCode;` |
|     515 |  556 | `		PH7_MemObjInit(pCtx->pVm,&sCode);` |
|     515 |  557 | `		PH7_MemObjStore(apArg[1],&sCode);` |
|     515 |  558 | `		PH7_MemObjToInteger(&sCode);` |
|     515 |  559 | `		if( sCode.x.iVal != 0 ){` |
|     488 |  560 | `			PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_CODE,sCode.x.iVal);` |
|     243 |  561 | `		}` |
|     515 |  562 | `		PH7_MemObjRelease(&sCode);` |
|     255 |  563 | `	}` |
|       - |  564 | ``	/* php's `previous` is the LAST parameter of each constructor, and`` |
|       - |  565 | `	 * ErrorException's is #5 rather than #2. */` |
| 1465616 |  566 | `	if( nArg > iPrev && (apArg[iPrev]->iFlags & MEMOBJ_OBJ) && apArg[iPrev]->x.pOther ){` |
|      32 |  567 | `		PH7_NativeSetAttrObj(pCtx->pVm,pThis,EXC_PREVIOUS,` |
|      18 |  568 | `			(ph7_class_instance *)apArg[iPrev]->x.pOther);` |
|       9 |  569 | `	}` |
| 1465616 |  570 | `}` |
| 1465595 |  571 | `static int vm_builtin_Exception_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  572 | `{` |
| 1465600 |  573 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
| 1465600 |  574 | `	if( pThis ){` |
| 1465600 |  575 | `		VmExcInitProps(pCtx,pThis,nArg,apArg,2);` |
|  732770 |  576 | `	}` |
| 1465600 |  577 | `	return PH7_OK;` |
|       5 |  578 | `}` |
|       - |  579 | `/*` |
|       - |  580 | ` * ErrorException's own constructor: php's Exception three, then severity, then` |
|       - |  581 | `` * the OPTIONAL file/line overrides. php's `?string $filename = null` /`` |
|       - |  582 | `` * `?int $line = null` mean "keep the creation site" — the chunk defaulted them to`` |
|       - |  583 | ` * __FILE__/__LINE__, which resolved against the EMBEDDED chunk and reported` |
|       - |  584 | `` * `:MEMORY:` line 1 for every ErrorException that did not pass them. php's one`` |
|       - |  585 | ` * asymmetry: a filename WITHOUT a line resets the line to 0.` |
|       - |  586 | ` */` |
|      16 |  587 | `static int vm_builtin_ErrorException_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  588 | `{` |
|      17 |  589 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      17 |  590 | `	if( pThis == 0 ){` |
|     ! 0 |  591 | `		return PH7_OK;` |
|       - |  592 | `	}` |
|      17 |  593 | `	VmExcInitProps(pCtx,pThis,nArg,apArg,5);` |
|      17 |  594 | `	if( nArg > 2 ){` |
|      11 |  595 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_SEVERITY,ph7_value_to_int64(apArg[2]));` |
|       5 |  596 | `	}` |
|      17 |  597 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|       9 |  598 | `		int nFile = 0;` |
|       9 |  599 | `		const char *zFile = ph7_value_to_string(apArg[3],&nFile);` |
|       9 |  600 | `		PH7_NativeSetAttrStr(pCtx->pVm,pThis,EXC_FILE,zFile,nFile);` |
|       9 |  601 | `		if( nArg < 5 \|\| ph7_value_is_null(apArg[4]) ){` |
|       3 |  602 | `			PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_LINE,0);` |
|       1 |  603 | `		}` |
|       4 |  604 | `	}` |
|      17 |  605 | `	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|       7 |  606 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_LINE,ph7_value_to_int64(apArg[4]));` |
|       3 |  607 | `	}` |
|      17 |  608 | `	return PH7_OK;` |
|       9 |  609 | `}` |
|       - |  610 | `/*` |
|       - |  611 | ` * php's private __clone. It has an empty body and is never reached: the class` |
|       - |  612 | ` * carries php's own clone refusal (PH7_CLASS_NOCLONE, answered before any body` |
|       - |  613 | `` * runs), which is what `clone $e` reports — "Trying to clone an uncloneable`` |
|       - |  614 | ` * object of class X", not a visibility error. Declaring it is still php-visible:` |
|       - |  615 | `` * Reflection lists it, and `$e->__clone()` from inside the class works.`` |
|       - |  616 | ` */` |
|     ! 0 |  617 | `static int vm_builtin_Exception_clone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  618 | `{` |
|     ! 0 |  619 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  620 | `	ph7_result_null(pCtx);` |
|     ! 0 |  621 | `	return PH7_OK;` |
|     ! 0 |  622 | `}` |
|       - |  623 | `/*` |
|       - |  624 | ` * php's __wakeup: the two UNTYPED slots are the only ones a serialized payload` |
|       - |  625 | ` * can lie about (the other five are typed and the store enforces them), so php` |
|       - |  626 | ` * DROPS a message that is not a string and a code that is not an int rather than` |
|       - |  627 | ` * letting a method read one.` |
|       - |  628 | ` */` |
|     ! 0 |  629 | `static void VmExcDropSlot(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot)` |
|     ! 0 |  630 | `{` |
|     ! 0 |  631 | `	SyHashEntry *pEntry = SyHashGet(&pThis->hAttr,(const void *)zSlot,SyStrlen(zSlot));` |
|     ! 0 |  632 | `	if( pEntry ){` |
|     ! 0 |  633 | `		PH7_VmReleaseInstanceAttr(&(*pVm),(VmClassAttr *)pEntry->pUserData);` |
|     ! 0 |  634 | `		PH7_ClassInstanceDeleteAttrEntry(pThis,pEntry);` |
|     ! 0 |  635 | `	}` |
|     ! 0 |  636 | `}` |
|     ! 0 |  637 | `static int vm_builtin_Exception_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  638 | `{` |
|     ! 0 |  639 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - |  640 | `	ph7_value *pVal;` |
|     ! 0 |  641 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  642 | `	if( pThis == 0 ){` |
|     ! 0 |  643 | `		return PH7_OK;` |
|       - |  644 | `	}` |
|     ! 0 |  645 | `	pVal = PH7_NativeAttr(pThis,EXC_MESSAGE);` |
|     ! 0 |  646 | `	if( pVal && (pVal->iFlags & MEMOBJ_NULL) == 0 && (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 |  647 | `		VmExcDropSlot(pCtx->pVm,pThis,EXC_MESSAGE);` |
|     ! 0 |  648 | `	}` |
|     ! 0 |  649 | `	pVal = PH7_NativeAttr(pThis,EXC_CODE);` |
|     ! 0 |  650 | `	if( pVal && (pVal->iFlags & MEMOBJ_NULL) == 0 && (pVal->iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 |  651 | `		VmExcDropSlot(pCtx->pVm,pThis,EXC_CODE);` |
|     ! 0 |  652 | `	}` |
|     ! 0 |  653 | `	ph7_result_null(pCtx);` |
|     ! 0 |  654 | `	return PH7_OK;` |
|     ! 0 |  655 | `}` |
|       - |  656 | `/*` |
|       - |  657 | ` * One argument of a trace frame, php's smart_str_append_scalar: a string is` |
|       - |  658 | `` * single-quoted, ESCAPED (`\n`, `\xNN` for anything non-printable) and truncated`` |
|       - |  659 | `` * to 15 bytes with `...` inside the quotes; a float takes php's precision; an`` |
|       - |  660 | `` * enum case prints `Enum::Case`; and anything else is a bare word.`` |
|       - |  661 | ` */` |
|       - |  662 | `#define EXC_ARG_MAX 15` |
|     106 |  663 | `static void VmExcTraceArg(ph7_vm *pVm,SyBlob *pOut,ph7_value *pArg)` |
|       2 |  664 | `{` |
|     108 |  665 | `	if( pArg == 0 \|\| (pArg->iFlags & MEMOBJ_NULL) ){` |
|       3 |  666 | `		SyBlobAppend(pOut,"NULL",sizeof("NULL")-1);` |
|       3 |  667 | `		return;` |
|       - |  668 | `	}` |
|     106 |  669 | `	if( pArg->iFlags & MEMOBJ_BOOL ){` |
|       5 |  670 | `		if( pArg->x.iVal ){` |
|       3 |  671 | `			SyBlobAppend(pOut,"true",sizeof("true")-1);` |
|       2 |  672 | `		}else{` |
|       3 |  673 | `			SyBlobAppend(pOut,"false",sizeof("false")-1);` |
|       - |  674 | `		}` |
|       5 |  675 | `		return;` |
|       - |  676 | `	}` |
|     102 |  677 | `	if( pArg->iFlags & MEMOBJ_HASHMAP ){` |
|       3 |  678 | `		SyBlobAppend(pOut,"Array",sizeof("Array")-1);` |
|       3 |  679 | `		return;` |
|       - |  680 | `	}` |
|     100 |  681 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|       3 |  682 | `		ph7_class_instance *pObj = (ph7_class_instance *)pArg->x.pOther;` |
|       3 |  683 | `		if( pObj && pObj->pClass && (pObj->pClass->iFlags & PH7_CLASS_ENUM) ){` |
|     ! 0 |  684 | `			ph7_value *pName = PH7_NativeAttr(pObj,"name");` |
|     ! 0 |  685 | `			SyBlobFormat(pOut,"%z::",&pObj->pClass->sName);` |
|     ! 0 |  686 | `			if( pName ){` |
|     ! 0 |  687 | `				SyBlobAppend(pOut,SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|     ! 0 |  688 | `			}` |
|     ! 0 |  689 | `			return;` |
|       - |  690 | `		}` |
|       3 |  691 | `		SyBlobAppend(pOut,"Object(",sizeof("Object(")-1);` |
|       3 |  692 | `		if( pObj && pObj->pClass ){` |
|       3 |  693 | `			SyBlobFormat(pOut,"%z",&pObj->pClass->sName);` |
|       1 |  694 | `		}` |
|       3 |  695 | `		SyBlobAppend(pOut,")",sizeof(")")-1);` |
|       3 |  696 | `		return;` |
|       - |  697 | `	}` |
|      98 |  698 | `	if( pArg->iFlags & MEMOBJ_STRING ){` |
|       - |  699 | `		/* php 8.5 does not put string CONTENT in a trace at all: every non-empty` |
|       - |  700 | `		 * one renders as '...' (the empty one still shows as ''), so a password` |
|       - |  701 | `		 * or a token passed to the function that threw cannot reach a log through` |
|       - |  702 | `		 * the trace. The truncate-at-15-and-escape shape here was php 8.4's. */` |
|      52 |  703 | `		if( SyBlobLength(&pArg->sBlob) < 1 ){` |
|       3 |  704 | `			SyBlobAppend(pOut,"''",sizeof("''")-1);` |
|       2 |  705 | `		}else{` |
|      50 |  706 | `			SyBlobAppend(pOut,"'...'",sizeof("'...'")-1);` |
|       - |  707 | `		}` |
|      52 |  708 | `		return;` |
|       - |  709 | `	}` |
|       - |  710 | `	{` |
|       - |  711 | `		/* int / float / anything else: php prints the scalar itself -- but a` |
|       - |  712 | `		 * trace FLOAT always shows its fraction (1.0, not the "1" the ordinary` |
|       - |  713 | `		 * string cast produces), which is what tells a float argument apart from` |
|       - |  714 | `		 * an int one. INF/NAN and the exponent forms already carry a marker. */` |
|       - |  715 | `		ph7_value sTmp;` |
|       - |  716 | `		const char *z;` |
|       - |  717 | `		sxu32 n,i;` |
|      47 |  718 | `		int bMarked = 0;` |
|      47 |  719 | `		PH7_MemObjInit(&(*pVm),&sTmp);` |
|      47 |  720 | `		PH7_MemObjStore(pArg,&sTmp);` |
|      47 |  721 | `		PH7_MemObjToString(&sTmp);` |
|      47 |  722 | `		z = (const char *)SyBlobData(&sTmp.sBlob);` |
|      47 |  723 | `		n = SyBlobLength(&sTmp.sBlob);` |
|      47 |  724 | `		SyBlobAppend(pOut,z,n);` |
|      47 |  725 | `		if( pArg->iFlags & MEMOBJ_REAL ){` |
|      23 |  726 | `			for( i = 0 ; i < n ; ++i ){` |
|      19 |  727 | `				if( z[i] < '0' \|\| z[i] > '9' ){` |
|      11 |  728 | `					if( z[i] != '-' && z[i] != '+' ){` |
|       9 |  729 | `						bMarked = 1;` |
|       9 |  730 | `						break;` |
|       - |  731 | `					}` |
|       1 |  732 | `				}` |
|       6 |  733 | `			}` |
|      13 |  734 | `			if( !bMarked ){` |
|       5 |  735 | `				SyBlobAppend(pOut,".0",sizeof(".0")-1);` |
|       2 |  736 | `			}` |
|       6 |  737 | `		}` |
|      47 |  738 | `		PH7_MemObjRelease(&sTmp);` |
|       - |  739 | `	}` |
|      55 |  740 | `}` |
|       - |  741 | `/* An element of a trace frame, or NULL when the frame does not carry it. */` |
|    1152 |  742 | `static ph7_value * VmExcFrameField(ph7_vm *pVm,ph7_hashmap *pFrame,const char *zField)` |
|       5 |  743 | `{` |
|    1157 |  744 | `	ph7_hashmap_node *pNode = 0;` |
|       - |  745 | `	ph7_value sKey;` |
|       - |  746 | `	sxi32 rc;` |
|       - |  747 | `	SyString sName;` |
|    1157 |  748 | `	SyStringInitFromBuf(&sName,zField,SyStrlen(zField));` |
|    1157 |  749 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,&sName);` |
|    1157 |  750 | `	rc = PH7_HashmapLookup(pFrame,&sKey,&pNode);` |
|    1157 |  751 | `	PH7_MemObjRelease(&sKey);` |
|    1157 |  752 | `	if( rc != SXRET_OK \|\| pNode == 0 ){` |
|     441 |  753 | `		return 0;` |
|       - |  754 | `	}` |
|     721 |  755 | `	return (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|     581 |  756 | `}` |
|     774 |  757 | `static void VmExcFrameStr(SyBlob *pOut,ph7_value *pVal)` |
|       5 |  758 | `{` |
|     779 |  759 | `	if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){` |
|     443 |  760 | `		SyBlobAppend(pOut,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|     219 |  761 | `	}` |
|     779 |  762 | `}` |
|       - |  763 | `/* A slot's string form, taken through a COPY: converting the value in place` |
|       - |  764 | ` * would rewrite the exception's own state. */` |
|       6 |  765 | `static void VmExcValueStr(ph7_vm *pVm,ph7_value *pVal,SyBlob *pOut)` |
|       1 |  766 | `{` |
|       - |  767 | `	ph7_value sTmp;` |
|       7 |  768 | `	if( pVal == 0 ){` |
|     ! 0 |  769 | `		return;` |
|       - |  770 | `	}` |
|       7 |  771 | `	PH7_MemObjInit(&(*pVm),&sTmp);` |
|       7 |  772 | `	PH7_MemObjStore(pVal,&sTmp);` |
|       7 |  773 | `	PH7_MemObjToString(&sTmp);` |
|       7 |  774 | `	SyBlobAppend(pOut,SyBlobData(&sTmp.sBlob),SyBlobLength(&sTmp.sBlob));` |
|       7 |  775 | `	PH7_MemObjRelease(&sTmp);` |
|       4 |  776 | `}` |
|       - |  777 | ``/* Does the blob contain this literal? SyBlobSearch() is `#ifndef`` |
|       - |  778 | `` * PH7_DISABLE_BUILTIN_FUNC`, and the exception family exists in the tiny build`` |
|       - |  779 | ` * too, so the one search this file needs is spelled out. */` |
|     ! 0 |  780 | `static int VmExcBlobHas(SyBlob *pBlob,const char *zPat,sxu32 nPat)` |
|     ! 0 |  781 | `{` |
|     ! 0 |  782 | `	const char *z = (const char *)SyBlobData(pBlob);` |
|     ! 0 |  783 | `	sxu32 n = SyBlobLength(pBlob);` |
|       - |  784 | `	sxu32 i;` |
|     ! 0 |  785 | `	if( nPat == 0 \|\| n < nPat ){` |
|     ! 0 |  786 | `		return 0;` |
|       - |  787 | `	}` |
|     ! 0 |  788 | `	for( i = 0 ; i + nPat <= n ; i++ ){` |
|     ! 0 |  789 | `		if( SyMemcmp((const void *)&z[i],(const void *)zPat,nPat) == 0 ){` |
|     ! 0 |  790 | `			return 1;` |
|       - |  791 | `		}` |
|     ! 0 |  792 | `	}` |
|     ! 0 |  793 | `	return 0;` |
|     ! 0 |  794 | `}` |
|       - |  795 | ``/* php's `Z_OBJCE_P == zend_ce_type_error \|\| == zend_ce_argument_count_error`:`` |
|       - |  796 | ` * the two classes whose message __toString finishes with " and defined". */` |
|       6 |  797 | `static int VmExcIsArgError(ph7_vm *pVm,ph7_class_instance *pExc)` |
|       1 |  798 | `{` |
|       7 |  799 | `	ph7_class *pClass = pExc ? pExc->pClass : 0;` |
|       - |  800 | `	ph7_class *pType;` |
|       7 |  801 | `	if( pClass == 0 ){` |
|     ! 0 |  802 | `		return 0;` |
|       - |  803 | `	}` |
|       7 |  804 | `	pType = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,FALSE,0);` |
|       7 |  805 | `	if( pType && pClass == pType ){` |
|     ! 0 |  806 | `		return 1;` |
|       - |  807 | `	}` |
|       7 |  808 | `	pType = PH7_VmExtractClass(&(*pVm),"ArgumentCountError",sizeof("ArgumentCountError")-1,FALSE,0);` |
|       7 |  809 | `	return pType != 0 && pClass == pType;` |
|       4 |  810 | `}` |
|       - |  811 | `/*` |
|       - |  812 | `` * php's zend_trace_to_string: one `#N file(line): Class->method(args)` line per`` |
|       - |  813 | `` * frame, then `#N {main}` with NO trailing newline. A frame with no `file` is`` |
|       - |  814 | `` * php's `[internal function]: `.`` |
|       - |  815 | ` */` |
|     694 |  816 | `PH7_PRIVATE void PH7_VmTraceToString(ph7_vm *pVm,ph7_value *pTrace,int bMainMarker,SyBlob *pOut)` |
|       5 |  817 | `{` |
|       - |  818 | `	ph7_hashmap *pMap;` |
|       - |  819 | `	ph7_hashmap_node *pEntry;` |
|     699 |  820 | `	sxu32 nFrame = 0;` |
|     699 |  821 | `	if( pTrace && (pTrace->iFlags & MEMOBJ_HASHMAP) && pTrace->x.pOther ){` |
|     699 |  822 | `		pMap = (ph7_hashmap *)pTrace->x.pOther;` |
|       - |  823 | `		/* Insertion order is pFirst then the pPrev chain (rule 12). */` |
|     891 |  824 | `		for( pEntry = pMap->pFirst ; pEntry ; pEntry = pEntry->pPrev ){` |
|     197 |  825 | `			ph7_value *pFrameVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pEntry->nValIdx);` |
|       - |  826 | `			ph7_hashmap *pFrame;` |
|       - |  827 | `			ph7_value *pFile;` |
|     197 |  828 | `			if( pFrameVal == 0 \|\| (pFrameVal->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 |  829 | `				continue;` |
|       - |  830 | `			}` |
|     197 |  831 | `			pFrame = (ph7_hashmap *)pFrameVal->x.pOther;` |
|     197 |  832 | `			SyBlobFormat(pOut,"#%u ",nFrame);` |
|     197 |  833 | `			pFile = VmExcFrameField(&(*pVm),pFrame,"file");` |
|     293 |  834 | `			if( pFile && (pFile->iFlags & MEMOBJ_STRING) ){` |
|     197 |  835 | `				ph7_value *pLine = VmExcFrameField(&(*pVm),pFrame,"line");` |
|     197 |  836 | `				VmExcFrameStr(pOut,pFile);` |
|     389 |  837 | `				SyBlobFormat(pOut,"(%qd): ",` |
|     192 |  838 | `					(pLine && (pLine->iFlags & MEMOBJ_INT)) ? pLine->x.iVal : (sxi64)0);` |
|     101 |  839 | `			}else{` |
|     ! 0 |  840 | `				SyBlobAppend(pOut,"[internal function]: ",sizeof("[internal function]: ")-1);` |
|       - |  841 | `			}` |
|     197 |  842 | `			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"class"));` |
|     197 |  843 | `			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"type"));` |
|     197 |  844 | `			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"function"));` |
|     197 |  845 | `			SyBlobAppend(pOut,"(",sizeof("(")-1);` |
|       - |  846 | `			{` |
|     197 |  847 | `				ph7_value *pArgs = VmExcFrameField(&(*pVm),pFrame,"args");` |
|     197 |  848 | `				if( pArgs && (pArgs->iFlags & MEMOBJ_HASHMAP) && pArgs->x.pOther ){` |
|      94 |  849 | `					ph7_hashmap *pArgMap = (ph7_hashmap *)pArgs->x.pOther;` |
|       - |  850 | `					ph7_hashmap_node *pArg;` |
|      94 |  851 | `					int bFirst = 1;` |
|     200 |  852 | `					for( pArg = pArgMap->pFirst ; pArg ; pArg = pArg->pPrev ){` |
|     108 |  853 | `						if( !bFirst ){` |
|      17 |  854 | `							SyBlobAppend(pOut,", ",sizeof(", ")-1);` |
|       8 |  855 | `						}` |
|     108 |  856 | `						bFirst = 0;` |
|     161 |  857 | `						VmExcTraceArg(&(*pVm),pOut,` |
|      53 |  858 | `							(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pArg->nValIdx));` |
|      55 |  859 | `					}` |
|      46 |  860 | `				}` |
|       - |  861 | `			}` |
|     197 |  862 | `			SyBlobAppend(pOut,")\n",sizeof(")\n")-1);` |
|     197 |  863 | `			nFrame++;` |
|     101 |  864 | `		}` |
|     347 |  865 | `	}` |
|     699 |  866 | `	if( bMainMarker ){` |
|       - |  867 | `		/* getTraceAsString() ends on the bottom marker; debug_print_backtrace()` |
|       - |  868 | `		 * does not print one -- it stops after the last real frame. */` |
|     657 |  869 | `		SyBlobFormat(pOut,"#%u {main}",nFrame);` |
|     326 |  870 | `	}` |
|     699 |  871 | `}` |
|     632 |  872 | `static int vm_builtin_Exception_getTraceAsString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  873 | `{` |
|     637 |  874 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - |  875 | `	SyBlob sOut;` |
|     316 |  876 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     637 |  877 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     637 |  878 | `	PH7_VmTraceToString(pCtx->pVm,pThis ? PH7_NativeAttr(pThis,EXC_TRACE) : 0,TRUE,&sOut);` |
|     637 |  879 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     637 |  880 | `	SyBlobRelease(&sOut);` |
|     637 |  881 | `	return PH7_OK;` |
|       5 |  882 | `}` |
|       - |  883 | `/*` |
|       - |  884 | ` * php's Exception::__toString.` |
|       - |  885 | ` *` |
|       - |  886 | ` *    C: message in file:line` |
|       - |  887 | ` *    Stack trace:` |
|       - |  888 | ` *    <trace>` |
|       - |  889 | ` *` |
|       - |  890 | ` * The PREVIOUS chain is part of the format and the ORDER is inverted: php builds` |
|       - |  891 | `` * the string innermost-first and joins the shallower ones after `\n\nNext `, so`` |
|       - |  892 | ` * the root cause is printed first. The chunk answered a four-field space-joined` |
|       - |  893 | `` * line instead — `file line code message` — which no php ever produced, and it is`` |
|       - |  894 | `` * what an uncaught exception, `echo $e` and `(string)$e` all show.`` |
|       - |  895 | ` *` |
|       - |  896 | ` * The walk carries its ancestors on the C stack (rule 31): php protects each` |
|       - |  897 | `` * object it visits and stops when it comes back round, and a `$a->previous = $b;`` |
|       - |  898 | `` * $b->previous = $a` pair must not spin.`` |
|       - |  899 | ` */` |
|       - |  900 | `#define EXC_CHAIN_MAX 256` |
|       4 |  901 | `static int vm_builtin_Exception_toString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  902 | `{` |
|       - |  903 | `	ph7_class_instance *apChain[EXC_CHAIN_MAX];` |
|       5 |  904 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       5 |  905 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - |  906 | `	SyBlob sOut;` |
|       5 |  907 | `	int nChain = 0;` |
|       - |  908 | `	int i,j;` |
|       2 |  909 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      11 |  910 | `	while( pThis && nChain < EXC_CHAIN_MAX ){` |
|       - |  911 | `		ph7_class_instance *pPrev;` |
|       9 |  912 | `		for( j = 0 ; j < nChain ; j++ ){` |
|       3 |  913 | `			if( apChain[j] == pThis ){` |
|     ! 0 |  914 | `				pThis = 0;    /* already on the chain: php's recursion protection */` |
|     ! 0 |  915 | `				break;` |
|       - |  916 | `			}` |
|       2 |  917 | `		}` |
|       7 |  918 | `		if( pThis == 0 ){` |
|     ! 0 |  919 | `			break;` |
|       - |  920 | `		}` |
|       7 |  921 | `		apChain[nChain++] = pThis;` |
|       7 |  922 | `		pPrev = PH7_NativeAttrObj(pThis,EXC_PREVIOUS);` |
|       7 |  923 | `		pThis = pPrev;` |
|       1 |  924 | `	}` |
|       - |  925 | `	/* php formats the SHALLOWEST first and pushes each one it has already built` |
|       - |  926 | `	 * behind the next, so the printed order is inverted: the ROOT CAUSE leads and` |
|       - |  927 | ``	 * every caller follows it after `\n\nNext `. */`` |
|       5 |  928 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      11 |  929 | `	for( i = 0 ; i < nChain ; i++ ){` |
|       7 |  930 | `		ph7_class_instance *pExc = apChain[i];` |
|       7 |  931 | `		ph7_value *pLine = PH7_NativeAttr(pExc,EXC_LINE);` |
|       - |  932 | `		SyBlob sMsg;` |
|       - |  933 | `		SyBlob sThis;` |
|       7 |  934 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       7 |  935 | `		SyBlobInit(&sThis,&pVm->sAllocator);` |
|       7 |  936 | `		VmExcValueStr(pVm,PH7_NativeAttr(pExc,EXC_MESSAGE),&sMsg);` |
|       - |  937 | `		/* php's one message rewrite: a TypeError/ArgumentCountError raised at a` |
|       - |  938 | `		 * CALL SITE says "..., called in F on line N", and __toString finishes the` |
|       - |  939 | `		 * sentence with " and defined". */` |
|       6 |  940 | `		if( VmExcIsArgError(pVm,pExc)` |
|       4 |  941 | `		 && VmExcBlobHas(&sMsg,", called in ",sizeof(", called in ")-1) ){` |
|     ! 0 |  942 | `			SyBlobAppend(&sMsg," and defined",sizeof(" and defined")-1);` |
|     ! 0 |  943 | `		}` |
|       7 |  944 | `		SyBlobFormat(&sThis,"%z",&pExc->pClass->sName);` |
|       7 |  945 | `		if( SyBlobLength(&sMsg) > 0 ){` |
|       5 |  946 | `			SyBlobAppend(&sThis,": ",sizeof(": ")-1);` |
|       5 |  947 | `			SyBlobAppend(&sThis,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       2 |  948 | `		}` |
|       7 |  949 | `		SyBlobAppend(&sThis," in ",sizeof(" in ")-1);` |
|       7 |  950 | `		VmExcFrameStr(&sThis,PH7_NativeAttr(pExc,EXC_FILE));` |
|      10 |  951 | `		SyBlobFormat(&sThis,":%qd\nStack trace:\n",` |
|       6 |  952 | `			(pLine && (pLine->iFlags & MEMOBJ_INT)) ? pLine->x.iVal : (sxi64)0);` |
|       7 |  953 | `		PH7_VmTraceToString(pVm,PH7_NativeAttr(pExc,EXC_TRACE),TRUE,&sThis);` |
|       7 |  954 | `		if( SyBlobLength(&sOut) > 0 ){` |
|       3 |  955 | `			SyBlobAppend(&sThis,"\n\nNext ",sizeof("\n\nNext ")-1);` |
|       3 |  956 | `			SyBlobAppend(&sThis,SyBlobData(&sOut),SyBlobLength(&sOut));` |
|       1 |  957 | `		}` |
|       7 |  958 | `		SyBlobReset(&sOut);` |
|       7 |  959 | `		SyBlobAppend(&sOut,SyBlobData(&sThis),SyBlobLength(&sThis));` |
|       7 |  960 | `		SyBlobRelease(&sMsg);` |
|       7 |  961 | `		SyBlobRelease(&sThis);` |
|       4 |  962 | `	}` |
|       5 |  963 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|       5 |  964 | `	SyBlobRelease(&sOut);` |
|       5 |  965 | `	return PH7_OK;` |
|       1 |  966 | `}` |
|       - |  967 | `/*` |
|       - |  968 | ` * The declaration. php's two roots carry the same eleven methods and the same` |
|       - |  969 | `` * seven slots; the only difference php's stub records is `Error::$line`, which`` |
|       - |  970 | ` * has NO default where Exception's is 0.` |
|       - |  971 | ` *` |
|       - |  972 | `` * PH7_CLASS_NOCLONE on EVERY row: php refuses `clone $e` outright, and a native`` |
|       - |  973 | ` * subclass does not inherit its parent's class flags (rule 29).` |
|       - |  974 | ` */` |
|       - |  975 | `#define EXC_METHODS(zCtor,xCtor) \` |
|       - |  976 | `	{ "__clone",          PH7_MOD_PRIVATE, "", "void", vm_builtin_Exception_clone }, \` |
|       - |  977 | `	{ "__construct",      PH7_MOD_PUBLIC, zCtor, 0, xCtor }, \` |
|       - |  978 | `	{ "__wakeup",         PH7_MOD_PUBLIC, "", "@void", vm_builtin_Exception_wakeup }, \` |
|       - |  979 | `	{ "getMessage",       PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "string", \` |
|       - |  980 | `	  vm_builtin_Exception_getMessage }, \` |
|       - |  981 | `	{ "getCode",          PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", 0, \` |
|       - |  982 | `	  vm_builtin_Exception_getCode }, \` |
|       - |  983 | `	{ "getFile",          PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "string", \` |
|       - |  984 | `	  vm_builtin_Exception_getFile }, \` |
|       - |  985 | `	{ "getLine",          PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "int", \` |
|       - |  986 | `	  vm_builtin_Exception_getLine }, \` |
|       - |  987 | `	{ "getTrace",         PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "array", \` |
|       - |  988 | `	  vm_builtin_Exception_getTrace }, \` |
|       - |  989 | `	{ "getPrevious",      PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "?Throwable", \` |
|       - |  990 | `	  vm_builtin_Exception_getPrevious }, \` |
|       - |  991 | `	{ "getTraceAsString", PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "string", \` |
|       - |  992 | `	  vm_builtin_Exception_getTraceAsString }, \` |
|       - |  993 | `	{ "__toString",       PH7_MOD_PUBLIC, "", "string", vm_builtin_Exception_toString }` |
|       - |  994 | `#define EXC_CTOR_SIG "string $message = \"\", int $code = 0, ?Throwable $previous = null"` |
|       - |  995 | `/* php's seven slots, twice: the only difference between the two roots is` |
|       - |  996 | `` * `Error::$line`, which php's stub declares with NO default where Exception's is`` |
|       - |  997 | `` * 0 (`PH7_NATIVE_VAL_NONE` — its hasDefaultValue() is false and the export`` |
|       - |  998 | `` * prints `protected int $line` bare). `message` and `code` are the two php leaves`` |
|       - |  999 | ` * UNTYPED, and its stub says why: BC, since a subclass may have assigned` |
|       - | 1000 | ` * anything to them. */` |
|       - | 1001 | `#define EXC_PROP_HEAD \` |
|       - | 1002 | `	{ EXC_MESSAGE,  PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 }, \` |
|       - | 1003 | `	{ EXC_STRING,   PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" }, \` |
|       - | 1004 | `	{ EXC_CODE,     PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 }, \` |
|       - | 1005 | `	{ EXC_FILE,     PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" }` |
|       - | 1006 | `#define EXC_PROP_TAIL \` |
|       - | 1007 | `	{ EXC_TRACE,    PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_ARRAY, 0, 0, 0.0 }, "array" }, \` |
|       - | 1008 | `	{ EXC_PREVIOUS, PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?Throwable" }` |
|    6721 | 1009 | `static sxi32 VmInstallExceptions(ph7_vm *pVm)` |
|       5 | 1010 | `{` |
|       - | 1011 | `	static const PH7_NativeMethodDef aExcMethod[] = {` |
|       - | 1012 | `		EXC_METHODS(EXC_CTOR_SIG,vm_builtin_Exception_construct)` |
|       - | 1013 | `	};` |
|       - | 1014 | `	static const PH7_NativePropDef aExcProp[] = {` |
|       - | 1015 | `		EXC_PROP_HEAD,` |
|       - | 1016 | `		{ EXC_LINE, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, "int" },` |
|       - | 1017 | `		EXC_PROP_TAIL` |
|       - | 1018 | `	};` |
|       - | 1019 | `	static const PH7_NativePropDef aErrProp[] = {` |
|       - | 1020 | `		EXC_PROP_HEAD,` |
|       - | 1021 | `		{ EXC_LINE, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|       - | 1022 | `		EXC_PROP_TAIL` |
|       - | 1023 | `	};` |
|       - | 1024 | `	static const PH7_NativePropDef aErrExcProp[] = {` |
|       - | 1025 | `		{ EXC_SEVERITY, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 1, 0, 0.0 }, "int" },` |
|       - | 1026 | `	};` |
|       - | 1027 | `	static const PH7_NativeMethodDef aErrExcMethod[] = {` |
|       - | 1028 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|       - | 1029 | `		  "string $message = \"\", int $code = 0, int $severity = E_ERROR, "` |
|       - | 1030 | `		  "?string $filename = null, ?int $line = null, ?Throwable $previous = null", 0,` |
|       - | 1031 | `		  vm_builtin_ErrorException_construct },` |
|       - | 1032 | `		{ "getSeverity", PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "int",` |
|       - | 1033 | `		  vm_builtin_ErrorException_getSeverity },` |
|       - | 1034 | `	};` |
|       - | 1035 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1036 | `		{ "Exception", 0, "Throwable", PH7_CLASS_NOCLONE,` |
|       - | 1037 | `		  aExcMethod, SX_ARRAYSIZE(aExcMethod), 0, 0, aExcProp, SX_ARRAYSIZE(aExcProp), 0, 0, 0 },` |
|       - | 1038 | `		{ "Error", 0, "Throwable", PH7_CLASS_NOCLONE,` |
|       - | 1039 | `		  aExcMethod, SX_ARRAYSIZE(aExcMethod), 0, 0, aErrProp, SX_ARRAYSIZE(aErrProp), 0, 0, 0 },` |
|       - | 1040 | `		/* Zend's own subclasses, then ErrorException, then SPL's tree. Every row is` |
|       - | 1041 | `		 * declaration-only in php too. */` |
|       - | 1042 | `		{ "TypeError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1043 | `		{ "ArgumentCountError", "TypeError", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1044 | `		{ "ValueError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1045 | `		{ "FiberError", "Error", 0,` |
|       - | 1046 | `		  PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1047 | `		{ "AssertionError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1048 | `		{ "ArithmeticError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1049 | `		{ "DivisionByZeroError", "ArithmeticError", 0, PH7_CLASS_NOCLONE,` |
|       - | 1050 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1051 | `		{ "UnhandledMatchError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1052 | `		{ "CompileError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1053 | `		{ "ParseError", "CompileError", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1054 | `		{ "ErrorException", "Exception", 0, PH7_CLASS_NOCLONE,` |
|       - | 1055 | `		  aErrExcMethod, SX_ARRAYSIZE(aErrExcMethod), 0, 0,` |
|       - | 1056 | `		  aErrExcProp, SX_ARRAYSIZE(aErrExcProp), 0, 0, 0 },` |
|       - | 1057 | `		{ "LogicException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1058 | `		{ "RuntimeException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1059 | `		{ "BadFunctionCallException", "LogicException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1060 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1061 | `		{ "BadMethodCallException", "BadFunctionCallException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1062 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1063 | `		{ "DomainException", "LogicException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1064 | `		{ "InvalidArgumentException", "LogicException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1065 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1066 | `		{ "LengthException", "LogicException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1067 | `		{ "OutOfRangeException", "LogicException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1068 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1069 | `		{ "OutOfBoundsException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1070 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1071 | `		{ "OverflowException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1072 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1073 | `		{ "RangeException", "RuntimeException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1074 | `		{ "UnderflowException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1075 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1076 | `		{ "UnexpectedValueException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1077 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1078 | `		{ "JsonException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1079 | `	};` |
|       - | 1080 | `	{` |
|    6726 | 1081 | `		sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|    6726 | 1082 | `		if( rc == SXRET_OK ){` |
|       - | 1083 | ``			/* php refuses `new FiberError` -- the engine is the only thing that`` |
|       - | 1084 | `			 * raises one -- and words the refusal per class. */` |
|    6726 | 1085 | `			ph7_class *pFe = PH7_VmExtractClass(&(*pVm),"FiberError",` |
|       - | 1086 | `				sizeof("FiberError")-1,FALSE,0);` |
|    6726 | 1087 | `			if( pFe ){` |
|    6726 | 1088 | `				pFe->zNewRefusal = "The \"FiberError\" class is reserved for internal use "` |
|       - | 1089 | `					"and cannot be manually instantiated";` |
|    3356 | 1090 | `			}` |
|    3356 | 1091 | `		}` |
|    6726 | 1092 | `		return rc;` |
|       - | 1093 | `	}` |
|       5 | 1094 | `}` |
|       - | 1095 | `/*` |
|       - | 1096 | ` * The eleven core interfaces, declared from C.` |
|       - | 1097 | ` *` |
|       - | 1098 | ` * They are contracts -- no method here has a body, every row is` |
|       - | 1099 | ` * PH7_MOD_ABSTRACT -- so the conversion is entirely about what the DECLARATION` |
|       - | 1100 | ` * says, which is where a chunk fell short in four php-visible ways:` |
|       - | 1101 | ` *` |
|       - | 1102 | `` *  - php's `interface Throwable extends Stringable`: the chunk redeclared`` |
|       - | 1103 | ` *    __toString() on Throwable instead, so no Exception was ever Stringable` |
|       - | 1104 | `` *    (`$e instanceof Stringable` was false, and Reflection attributed the`` |
|       - | 1105 | `` *    method to Throwable rather than printing php's `inherits Stringable`);`` |
|       - | 1106 | ` *  - php declares a RETURN TYPE on all but three of these methods and marks` |
|       - | 1107 | `` *    nearly all of them TENTATIVE (the leading `@`, rule 45) -- a chunk has no`` |
|       - | 1108 | ` *    way to say tentative at all;` |
|       - | 1109 | `` *  - php's `mixed` on ArrayAccess's offsets, which the chunk left untyped;`` |
|       - | 1110 | ` *  - method ORDER, which Reflection prints: php lists Throwable's getPrevious` |
|       - | 1111 | ` *    before getTraceAsString, and Iterator's as current/next/key/valid/rewind.` |
|       - | 1112 | ` *` |
|       - | 1113 | ` * Order within the table is php's stub order too; the declare-then-link phases` |
|       - | 1114 | ` * of PH7_InstallNativeClasses let Throwable name Stringable and Iterator name` |
|       - | 1115 | `` * Traversable regardless of row order. An interface's parent is `zParent`, not`` |
|       - | 1116 | `` * `zImplements` (Reflection walks pBase to attribute an inherited method).`` |
|       - | 1117 | ` */` |
|    6721 | 1118 | `static sxi32 VmInstallCoreInterfaces(ph7_vm *pVm)` |
|       5 | 1119 | `{` |
|       - | 1120 | `	static const PH7_NativeMethodDef aStringable[] = {` |
|       - | 1121 | `		{ "__toString", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1122 | `	};` |
|       - | 1123 | `	static const PH7_NativeMethodDef aThrowable[] = {` |
|       - | 1124 | `		/* Not one of these is tentative: php's Throwable is a real contract. */` |
|       - | 1125 | `		{ "getMessage",       PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1126 | `		{ "getCode",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|       - | 1127 | `		{ "getFile",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1128 | `		{ "getLine",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "int", 0 },` |
|       - | 1129 | `		{ "getTrace",         PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "array", 0 },` |
|       - | 1130 | `		{ "getPrevious",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "?Throwable", 0 },` |
|       - | 1131 | `		{ "getTraceAsString", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1132 | `	};` |
|       - | 1133 | `	static const PH7_NativeMethodDef aArrayAccess[] = {` |
|       - | 1134 | `		{ "offsetExists", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset", "@bool", 0 },` |
|       - | 1135 | `		{ "offsetGet",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset", "@mixed", 0 },` |
|       - | 1136 | `		{ "offsetSet",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset, mixed $value",` |
|       - | 1137 | `		  "@void", 0 },` |
|       - | 1138 | `		{ "offsetUnset",  PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset", "@void", 0 },` |
|       - | 1139 | `	};` |
|       - | 1140 | `	static const PH7_NativeMethodDef aCountable[] = {` |
|       - | 1141 | `		{ "count", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@int", 0 },` |
|       - | 1142 | `	};` |
|       - | 1143 | `	static const PH7_NativeMethodDef aJsonSerializable[] = {` |
|       - | 1144 | `		{ "jsonSerialize", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@mixed", 0 },` |
|       - | 1145 | `	};` |
|       - | 1146 | `	/* The concrete cases()/from()/tryFrom() an enum gets are native methods` |
|       - | 1147 | `	 * declared to match these (oo_native.c, PH7_InstallEnumInterfaceMethods). */` |
|       - | 1148 | `	static const PH7_NativeMethodDef aUnitEnum[] = {` |
|       - | 1149 | `		{ "cases", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_ABSTRACT, "", "array", 0 },` |
|       - | 1150 | `	};` |
|       - | 1151 | `	static const PH7_NativeMethodDef aBackedEnum[] = {` |
|       - | 1152 | `		{ "from",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_ABSTRACT, "string\|int $value",` |
|       - | 1153 | `		  "static", 0 },` |
|       - | 1154 | `		{ "tryFrom", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_ABSTRACT, "string\|int $value",` |
|       - | 1155 | `		  "?static", 0 },` |
|       - | 1156 | `	};` |
|       - | 1157 | `	static const PH7_NativeMethodDef aIterator[] = {` |
|       - | 1158 | `		{ "current", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@mixed", 0 },` |
|       - | 1159 | `		{ "next",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@void", 0 },` |
|       - | 1160 | `		{ "key",     PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@mixed", 0 },` |
|       - | 1161 | `		{ "valid",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@bool", 0 },` |
|       - | 1162 | `		{ "rewind",  PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@void", 0 },` |
|       - | 1163 | `	};` |
|       - | 1164 | `	static const PH7_NativeMethodDef aIteratorAggregate[] = {` |
|       - | 1165 | `		{ "getIterator", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@Traversable", 0 },` |
|       - | 1166 | `	};` |
|       - | 1167 | `	/* php's legacy Serializable declares NO return type on either method. */` |
|       - | 1168 | `	static const PH7_NativeMethodDef aSerializable[] = {` |
|       - | 1169 | `		{ "serialize",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|       - | 1170 | `		{ "unserialize", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $data", 0, 0 },` |
|       - | 1171 | `	};` |
|       - | 1172 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1173 | `		{ "Traversable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1174 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1175 | `		{ "Stringable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1176 | `		  aStringable, SX_ARRAYSIZE(aStringable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1177 | `		{ "Throwable", "Stringable", 0, PH7_CLASS_INTERFACE,` |
|       - | 1178 | `		  aThrowable, SX_ARRAYSIZE(aThrowable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1179 | `		{ "ArrayAccess", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1180 | `		  aArrayAccess, SX_ARRAYSIZE(aArrayAccess), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1181 | `		{ "Countable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1182 | `		  aCountable, SX_ARRAYSIZE(aCountable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1183 | `		{ "JsonSerializable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1184 | `		  aJsonSerializable, SX_ARRAYSIZE(aJsonSerializable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1185 | `		{ "UnitEnum", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1186 | `		  aUnitEnum, SX_ARRAYSIZE(aUnitEnum), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1187 | `		{ "BackedEnum", "UnitEnum", 0, PH7_CLASS_INTERFACE,` |
|       - | 1188 | `		  aBackedEnum, SX_ARRAYSIZE(aBackedEnum), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1189 | `		{ "Iterator", "Traversable", 0, PH7_CLASS_INTERFACE,` |
|       - | 1190 | `		  aIterator, SX_ARRAYSIZE(aIterator), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1191 | `		{ "IteratorAggregate", "Traversable", 0, PH7_CLASS_INTERFACE,` |
|       - | 1192 | `		  aIteratorAggregate, SX_ARRAYSIZE(aIteratorAggregate), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1193 | `		{ "Serializable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1194 | `		  aSerializable, SX_ARRAYSIZE(aSerializable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1195 | `	};` |
|    6726 | 1196 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       5 | 1197 | `}` |
|       - | 1198 | `/*` |
|       - | 1199 | ` * ---------------------------------------------------------------------------` |
|       - | 1200 | `` * php's Directory — the object `dir()` answers.`` |
|       - | 1201 | ` *` |
|       - | 1202 | ` * php declares it FINAL with **no constructor at all**: the class is created by` |
|       - | 1203 | `` * `dir()` and `new Directory` is refused in the create_object handler, with a`` |
|       - | 1204 | ` * sentence that names dir() as the way to get one. Its two slots are` |
|       - | 1205 | `` * `public protected(set) readonly`, so a script can read `$d->path` and never`` |
|       - | 1206 | `` * write it, and its three methods declare return types (`read(): string\|false`).`` |
|       - | 1207 | ` * The chunk had a public constructor, a __destruct php does not declare, no` |
|       - | 1208 | ` * types anywhere and writable slots.` |
|       - | 1209 | ` * ---------------------------------------------------------------------------` |
|       - | 1210 | ` */` |
|       - | 1211 | `#define DIR_HANDLE "handle"` |
|       - | 1212 | `#define DIR_PATH   "path"` |
|       - | 1213 | `/*` |
|       - | 1214 | ` * Forward one method to the engine's own directory builtin (rule 7: call, don't` |
|       - | 1215 | `` * reimplement). php's Directory methods are `php_stream_readdir(...)` on the very`` |
|       - | 1216 | `` * stream `readdir()` uses, and a CLOSED handle is a TypeError there — the one`` |
|       - | 1217 | ` * place php's wording names the class rather than the function.` |
|       - | 1218 | ` */` |
|      40 | 1219 | `static int VmDirClosed(ph7_value *pHandle)` |
|       2 | 1220 | `{` |
|      42 | 1221 | `	io_private *pDev = (io_private *)pHandle->x.pOther;` |
|      42 | 1222 | `	return IO_PRIVATE_INVALID(pDev);` |
|       2 | 1223 | `}` |
|      40 | 1224 | `static int VmDirForward(ph7_context *pCtx,const char *zFunc,const char *zMethod)` |
|       2 | 1225 | `{` |
|      42 | 1226 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      42 | 1227 | `	ph7_value *pHandle = pThis ? PH7_NativeAttr(pThis,DIR_HANDLE) : 0;` |
|       - | 1228 | `	ph7_value *apArg[1];` |
|       - | 1229 | `	ph7_value sResult;` |
|       - | 1230 | `	ph7_value sName;` |
|       - | 1231 | `	SyString sStr;` |
|       - | 1232 | `	sxi32 rc;` |
|       - | 1233 | ``	/* php's check is `php_stream_from_zval` on a stream it CLOSED: closedir()`` |
|       - | 1234 | `	 * keeps the resource alive and marks it (gettype() answers` |
|       - | 1235 | `	 * "resource (closed)"), so the test is the magic, not the type. */` |
|      40 | 1236 | `	if( pHandle == 0 \|\| (pHandle->iFlags & MEMOBJ_RES) == 0` |
|      42 | 1237 | `	 \|\| VmDirClosed(pHandle) ){` |
|      10 | 1238 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1239 | `			"Directory::%s(): cannot use Directory resource after it has been closed",` |
|       3 | 1240 | `			zMethod);` |
|       - | 1241 | `	}` |
|      36 | 1242 | `	SyStringInitFromBuf(&sStr,zFunc,SyStrlen(zFunc));` |
|      36 | 1243 | `	PH7_MemObjInit(pCtx->pVm,&sName);` |
|      36 | 1244 | `	PH7_MemObjInitFromString(pCtx->pVm,&sName,&sStr);` |
|      36 | 1245 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      36 | 1246 | `	apArg[0] = pHandle;` |
|      36 | 1247 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,&sName,1,apArg,&sResult);` |
|      36 | 1248 | `	PH7_MemObjRelease(&sName);` |
|      36 | 1249 | `	if( rc == SXRET_OK ){` |
|      36 | 1250 | `		ph7_result_value(pCtx,&sResult);` |
|      17 | 1251 | `	}` |
|      36 | 1252 | `	PH7_MemObjRelease(&sResult);` |
|      36 | 1253 | `	return PH7_OK;` |
|      22 | 1254 | `}` |
|      28 | 1255 | `static int vm_builtin_Directory_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1256 | `{` |
|      14 | 1257 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      30 | 1258 | `	return VmDirForward(pCtx,"readdir","read");` |
|       2 | 1259 | `}` |
|       4 | 1260 | `static int vm_builtin_Directory_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1261 | `{` |
|       2 | 1262 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       5 | 1263 | `	return VmDirForward(pCtx,"rewinddir","rewind");` |
|       1 | 1264 | `}` |
|       8 | 1265 | `static int vm_builtin_Directory_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1266 | `{` |
|       4 | 1267 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      10 | 1268 | `	return VmDirForward(pCtx,"closedir","close");` |
|       2 | 1269 | `}` |
|    6721 | 1270 | `static sxi32 VmInstallDirectory(ph7_vm *pVm)` |
|       5 | 1271 | `{` |
|       - | 1272 | `	static const PH7_NativePropDef aDirProp[] = {` |
|       - | 1273 | `		{ DIR_PATH,   PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1274 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|       - | 1275 | `		{ DIR_HANDLE, PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1276 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "mixed" },` |
|       - | 1277 | `	};` |
|       - | 1278 | `	static const PH7_NativeMethodDef aDirMethod[] = {` |
|       - | 1279 | `		{ "close",  PH7_MOD_PUBLIC, "", "void", vm_builtin_Directory_close },` |
|       - | 1280 | `		{ "rewind", PH7_MOD_PUBLIC, "", "void", vm_builtin_Directory_rewind },` |
|       - | 1281 | `		{ "read",   PH7_MOD_PUBLIC, "", "string\|false", vm_builtin_Directory_read },` |
|       - | 1282 | `	};` |
|       - | 1283 | `	static const PH7_NativeClassSpec sSpec = {` |
|       - | 1284 | `		"Directory", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE,` |
|       - | 1285 | `		aDirMethod, SX_ARRAYSIZE(aDirMethod), 0, 0,` |
|       - | 1286 | `		aDirProp, SX_ARRAYSIZE(aDirProp), 0, 0, 0` |
|       - | 1287 | `	};` |
|    6726 | 1288 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|    6726 | 1289 | `	if( rc == SXRET_OK ){` |
|    6726 | 1290 | `		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"Directory",sizeof("Directory")-1,FALSE,0);` |
|    6726 | 1291 | `		if( pClass ){` |
|       - | 1292 | `			/* php words this refusal per class rather than with the generic` |
|       - | 1293 | `			 * "Instantiation of class %s is not allowed". */` |
|    6726 | 1294 | `			pClass->zNewRefusal = "Cannot directly construct Directory, use dir() instead";` |
|    3356 | 1295 | `		}` |
|    3356 | 1296 | `	}` |
|    6726 | 1297 | `	return rc;` |
|       5 | 1298 | `}` |
|       - | 1299 | `/*` |
|       - | 1300 | ` * ---------------------------------------------------------------------------` |
|       - | 1301 | ` * php's attribute classes.` |
|       - | 1302 | ` *` |
|       - | 1303 | ``  * Each carries an ATTRIBUTE of its own — `#[Attribute(Attribute::TARGET_CLASS)]` `` |
|       - | 1304 | ` * on Attribute, a target mask on every other one — and those records are` |
|       - | 1305 | ` * load-bearing rather than decorative: the engine reads them to decide whether a` |
|       - | 1306 | `` * user's `#[Deprecated]` may sit where it does, and ReflectionAttribute answers`` |
|       - | 1307 | ` * them. A compiled attribute holds its argument as byte-code, so this is what` |
|       - | 1308 | `` * `PH7_NativeClassAddAttribute()` exists for (rule 11's next unused corner,`` |
|       - | 1309 | ` * exercised here): the argument rides as a literal.` |
|       - | 1310 | ` *` |
|       - | 1311 | ` * php's Deprecated mask is 87 — TARGET_CLASS\|FUNCTION\|METHOD\|CLASS_CONSTANT\|` |
|       - | 1312 | ` * CONSTANT — where the chunk wrote 86 and left the CLASS bit out.` |
|       - | 1313 | ` *` |
|       - | 1314 | ` * Three of them declare NOTHING but their own mask, because what they mean is a` |
|       - | 1315 | `` * question something else asks: `#[AllowDynamicProperties]` is read by the`` |
|       - | 1316 | `` * dynamic-property decision at the write site, `#[SensitiveParameter]` by the`` |
|       - | 1317 | `` * backtrace builder, `#[ReturnTypeWillChange]` by php's tentative-return-type`` |
|       - | 1318 | ` * check (which the §10 non-deprecated policy removed, so nothing consults it` |
|       - | 1319 | ` * here). They still have to EXIST: a program that spells one and then asks` |
|       - | 1320 | `` * `getAttributes()[0]->newInstance()` gets php's object, not`` |
|       - | 1321 | `` * `Attribute class "AllowDynamicProperties" not found`.`` |
|       - | 1322 | ` *` |
|       - | 1323 | `` * `SensitiveParameterValue` is not an attribute at all — it is the box php puts`` |
|       - | 1324 | ` * a redacted argument in — but it belongs to the same feature and to the same` |
|       - | 1325 | ` * declaration site.` |
|       - | 1326 | ` * ---------------------------------------------------------------------------` |
|       - | 1327 | ` */` |
|       - | 1328 | ``/* php declares `public function __construct()` on the three marker attributes, so`` |
|       - | 1329 | `` * Reflection reports one and `new AllowDynamicProperties(1)` is an`` |
|       - | 1330 | ` * ArgumentCountError. The body has nothing to do: the object carries no state. */` |
|      10 | 1331 | `static int vm_builtin_AttrMarker_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1332 | `{` |
|       5 | 1333 | `	SXUNUSED(pCtx);` |
|       5 | 1334 | `	SXUNUSED(nArg);` |
|       5 | 1335 | `	SXUNUSED(apArg);` |
|      12 | 1336 | `	return PH7_OK;` |
|       2 | 1337 | `}` |
|       - | 1338 | `/* SensitiveParameterValue::__construct(mixed $value) / getValue() / __debugInfo() */` |
|       - | 1339 | `#define SPV_SLOT "value"` |
|      20 | 1340 | `static int vm_builtin_SensitiveParameterValue_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1341 | `{` |
|      21 | 1342 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      21 | 1343 | `	if( pThis && nArg > 0 ){` |
|      21 | 1344 | `		PH7_NativeSetProp(pCtx->pVm,pThis,SPV_SLOT,sizeof(SPV_SLOT)-1,apArg[0]);` |
|      10 | 1345 | `	}` |
|      21 | 1346 | `	return PH7_OK;` |
|       1 | 1347 | `}` |
|       6 | 1348 | `static int vm_builtin_SensitiveParameterValue_getValue(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1349 | `{` |
|       7 | 1350 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       7 | 1351 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,SPV_SLOT) : 0;` |
|       3 | 1352 | `	SXUNUSED(nArg);` |
|       3 | 1353 | `	SXUNUSED(apArg);` |
|       7 | 1354 | `	if( pVal ){` |
|       7 | 1355 | `		ph7_result_value(pCtx,pVal);` |
|       4 | 1356 | `	}else{` |
|     ! 0 | 1357 | `		ph7_result_null(pCtx);` |
|       - | 1358 | `	}` |
|       7 | 1359 | `	return PH7_OK;` |
|       1 | 1360 | `}` |
|       2 | 1361 | `static int vm_builtin_SensitiveParameterValue_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1362 | `{` |
|       3 | 1363 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|       1 | 1364 | `	SXUNUSED(nArg);` |
|       1 | 1365 | `	SXUNUSED(apArg);` |
|       3 | 1366 | `	if( pOut == 0 ){` |
|     ! 0 | 1367 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1368 | `	}` |
|       3 | 1369 | `	ph7_result_value(pCtx,pOut);` |
|       3 | 1370 | `	return PH7_OK;` |
|       2 | 1371 | `}` |
|       - | 1372 | `/*` |
|       - | 1373 | `` * php gives the class a `get_properties_for` handler that answers NULL for every`` |
|       - | 1374 | `` * purpose, so the box shows nothing to var_export, the `(array)` cast or`` |
|       - | 1375 | `` * json_encode either — not just to var_dump's `__debugInfo()`. The point of the`` |
|       - | 1376 | ` * class is that the value it holds does not leak onto a display surface.` |
|       - | 1377 | ` */` |
|       6 | 1378 | `static sxi32 VmPresentSensitiveParameterValue(ph7_vm *pVm,ph7_class_instance *pThis,` |
|       - | 1379 | `	ph7_value *pOut,int bDebug)` |
|       1 | 1380 | `{` |
|       3 | 1381 | `	SXUNUSED(pVm);` |
|       3 | 1382 | `	SXUNUSED(pThis);` |
|       3 | 1383 | `	SXUNUSED(pOut);` |
|       3 | 1384 | `	SXUNUSED(bDebug);` |
|       7 | 1385 | `	return SXRET_OK;   /* the empty shape, both handlers */` |
|       1 | 1386 | `}` |
|       8 | 1387 | `static int vm_builtin_Attribute_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1388 | `{` |
|       9 | 1389 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       9 | 1390 | `	if( pThis ){` |
|      16 | 1391 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,"flags",` |
|       7 | 1392 | `			nArg > 0 ? ph7_value_to_int64(apArg[0]) : 127);` |
|       4 | 1393 | `	}` |
|       9 | 1394 | `	return PH7_OK;` |
|       1 | 1395 | `}` |
|       - | 1396 | `/* NoDiscard::__construct(?string $message = null) */` |
|       2 | 1397 | `static int vm_builtin_NoDiscard_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1398 | `{` |
|       3 | 1399 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - | 1400 | `	ph7_value sVal;` |
|       3 | 1401 | `	if( pThis == 0 ){` |
|     ! 0 | 1402 | `		return PH7_OK;` |
|       - | 1403 | `	}` |
|       3 | 1404 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|       3 | 1405 | `	if( nArg > 0 ){` |
|     ! 0 | 1406 | `		PH7_MemObjStore(apArg[0],&sVal);` |
|     ! 0 | 1407 | `	}` |
|       3 | 1408 | `	PH7_NativeSetProp(pCtx->pVm,pThis,"message",sizeof("message")-1,&sVal);` |
|       3 | 1409 | `	PH7_MemObjRelease(&sVal);` |
|       3 | 1410 | `	return PH7_OK;` |
|       2 | 1411 | `}` |
|      10 | 1412 | `static int vm_builtin_Deprecated_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1413 | `{` |
|      11 | 1414 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - | 1415 | `	static const char *const azSlot[] = { "message", "since" };` |
|       - | 1416 | `	int n;` |
|      11 | 1417 | `	if( pThis == 0 ){` |
|     ! 0 | 1418 | `		return PH7_OK;` |
|       - | 1419 | `	}` |
|      31 | 1420 | `	for( n = 0 ; n < 2 ; n++ ){` |
|       - | 1421 | `		ph7_value sVal;` |
|      21 | 1422 | `		PH7_MemObjInit(pCtx->pVm,&sVal);` |
|      21 | 1423 | `		if( n < nArg ){` |
|      15 | 1424 | `			PH7_MemObjStore(apArg[n],&sVal);` |
|       7 | 1425 | `		}` |
|      21 | 1426 | `		PH7_NativeSetProp(pCtx->pVm,pThis,azSlot[n],SyStrlen(azSlot[n]),&sVal);` |
|      21 | 1427 | `		PH7_MemObjRelease(&sVal);` |
|      11 | 1428 | `	}` |
|      11 | 1429 | `	return PH7_OK;` |
|       6 | 1430 | `}` |
|    6721 | 1431 | `static sxi32 VmInstallAttributes(ph7_vm *pVm)` |
|       5 | 1432 | `{` |
|       - | 1433 | `	static const PH7_NativeConstDef aAttrConst[] = {` |
|       - | 1434 | `		{ "TARGET_CLASS",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },` |
|       - | 1435 | `		{ "TARGET_FUNCTION",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|       - | 1436 | `		{ "TARGET_METHOD",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4, 0, 0.0 },` |
|       - | 1437 | `		{ "TARGET_PROPERTY",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 8, 0, 0.0 },` |
|       - | 1438 | `		{ "TARGET_CLASS_CONSTANT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16, 0, 0.0 },` |
|       - | 1439 | `		{ "TARGET_PARAMETER",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },` |
|       - | 1440 | `		{ "TARGET_CONSTANT",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64, 0, 0.0 },` |
|       - | 1441 | `		{ "TARGET_ALL",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 127, 0, 0.0 },` |
|       - | 1442 | `		{ "IS_REPEATABLE",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 128, 0, 0.0 },` |
|       - | 1443 | `	};` |
|       - | 1444 | ``	/* php declares `public int $flags;` — typed, NO default (the constructor is`` |
|       - | 1445 | ``	 * the only writer), which is what the chunk's `public $flags;` could not say. */`` |
|       - | 1446 | `	static const PH7_NativePropDef aAttrProp[] = {` |
|       - | 1447 | `		{ "flags", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|       - | 1448 | `	};` |
|       - | 1449 | `	static const PH7_NativeMethodDef aAttrMethod[] = {` |
|       - | 1450 | `		{ "__construct", PH7_MOD_PUBLIC, "int $flags = Attribute::TARGET_ALL", 0,` |
|       - | 1451 | `		  vm_builtin_Attribute_construct },` |
|       - | 1452 | `	};` |
|       - | 1453 | `	static const PH7_NativePropDef aDepProp[] = {` |
|       - | 1454 | `		{ "message", PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1455 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "?string" },` |
|       - | 1456 | `		{ "since",   PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1457 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "?string" },` |
|       - | 1458 | `	};` |
|       - | 1459 | `	static const PH7_NativeMethodDef aDepMethod[] = {` |
|       - | 1460 | `		{ "__construct", PH7_MOD_PUBLIC, "?string $message = null, ?string $since = null", 0,` |
|       - | 1461 | `		  vm_builtin_Deprecated_construct },` |
|       - | 1462 | `	};` |
|       - | 1463 | ``	/* NoDiscard is Deprecated's shape minus the `since`. */`` |
|       - | 1464 | `	static const PH7_NativePropDef aNdProp[] = {` |
|       - | 1465 | `		{ "message", PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1466 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "?string" },` |
|       - | 1467 | `	};` |
|       - | 1468 | `	static const PH7_NativeMethodDef aNdMethod[] = {` |
|       - | 1469 | `		{ "__construct", PH7_MOD_PUBLIC, "?string $message = null", 0,` |
|       - | 1470 | `		  vm_builtin_NoDiscard_construct },` |
|       - | 1471 | `	};` |
|       - | 1472 | `	/* The three markers: one argless constructor each and no state at all. */` |
|       - | 1473 | `	static const PH7_NativeMethodDef aMarkerMethod[] = {` |
|       - | 1474 | `		{ "__construct", PH7_MOD_PUBLIC, "", 0, vm_builtin_AttrMarker_construct },` |
|       - | 1475 | `	};` |
|       - | 1476 | `	static const PH7_NativePropDef aSpvProp[] = {` |
|       - | 1477 | `		{ SPV_SLOT, PH7_MOD_PRIVATE\|PH7_MOD_READONLY,` |
|       - | 1478 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "mixed" },` |
|       - | 1479 | `	};` |
|       - | 1480 | `	static const PH7_NativeMethodDef aSpvMethod[] = {` |
|       - | 1481 | `		{ "__construct", PH7_MOD_PUBLIC, "mixed $value", 0,` |
|       - | 1482 | `		  vm_builtin_SensitiveParameterValue_construct },` |
|       - | 1483 | `		{ "getValue",    PH7_MOD_PUBLIC, "", "mixed",` |
|       - | 1484 | `		  vm_builtin_SensitiveParameterValue_getValue },` |
|       - | 1485 | `		{ "__debugInfo", PH7_MOD_PUBLIC, "", "array",` |
|       - | 1486 | `		  vm_builtin_SensitiveParameterValue_debugInfo },` |
|       - | 1487 | `	};` |
|       - | 1488 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1489 | `		{ "Attribute", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1490 | `		  aAttrMethod, SX_ARRAYSIZE(aAttrMethod), aAttrConst, SX_ARRAYSIZE(aAttrConst),` |
|       - | 1491 | `		  aAttrProp, SX_ARRAYSIZE(aAttrProp), 0, 0, 0 },` |
|       - | 1492 | `		{ "Deprecated", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1493 | `		  aDepMethod, SX_ARRAYSIZE(aDepMethod), 0, 0,` |
|       - | 1494 | `		  aDepProp, SX_ARRAYSIZE(aDepProp), 0, 0, 0 },` |
|       - | 1495 | `		{ "AllowDynamicProperties", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1496 | `		  aMarkerMethod, SX_ARRAYSIZE(aMarkerMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1497 | `		{ "SensitiveParameter", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1498 | `		  aMarkerMethod, SX_ARRAYSIZE(aMarkerMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1499 | `		{ "ReturnTypeWillChange", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1500 | `		  aMarkerMethod, SX_ARRAYSIZE(aMarkerMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1501 | `		{ "Override", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1502 | `		  aMarkerMethod, SX_ARRAYSIZE(aMarkerMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1503 | `		{ "NoDiscard", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1504 | `		  aNdMethod, SX_ARRAYSIZE(aNdMethod), 0, 0,` |
|       - | 1505 | `		  aNdProp, SX_ARRAYSIZE(aNdProp), 0, 0, 0 },` |
|       - | 1506 | `		/* php 8.5's marker for an attribute whose TARGET is checked late. It is` |
|       - | 1507 | `		 * the one attribute class php declares with no constructor at all --` |
|       - | 1508 | `		 * every other marker here has the empty one -- so a script writes it` |
|       - | 1509 | ``		 * bare and `newInstance()` builds it with nothing. */`` |
|       - | 1510 | `		{ "DelayedTargetValidation", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1511 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1512 | `		/* php refuses BOTH directions for the box (ZEND_ACC_NOT_SERIALIZABLE), which` |
|       - | 1513 | `		 * is the whole point: a redacted value must not reach a payload either. */` |
|       - | 1514 | `		{ "SensitiveParameterValue", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOSERIALIZE,` |
|       - | 1515 | `		  aSpvMethod, SX_ARRAYSIZE(aSpvMethod), 0, 0,` |
|       - | 1516 | `		  aSpvProp, SX_ARRAYSIZE(aSpvProp), 0, 0,` |
|       - | 1517 | `		  VmPresentSensitiveParameterValue },` |
|       - | 1518 | `	};` |
|       - | 1519 | ``	/* Each attribute class's own `#[Attribute(mask)]`, php's masks verbatim. The`` |
|       - | 1520 | `	 * literal rows are STATIC because PH7_NativeClassAddAttribute keeps a pointer` |
|       - | 1521 | `	 * to them for the VM's lifetime. */` |
|       - | 1522 | `	static const PH7_NativeAttrArg aMaskClass[]  = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 1,  0, 0.0 } } };` |
|       - | 1523 | `	static const PH7_NativeAttrArg aMaskDep[]    = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 87, 0, 0.0 } } };` |
|       - | 1524 | `	static const PH7_NativeAttrArg aMaskParam[]  = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 32, 0, 0.0 } } };` |
|       - | 1525 | `	static const PH7_NativeAttrArg aMaskMethod[] = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 4,  0, 0.0 } } };` |
|       - | 1526 | `	static const PH7_NativeAttrArg aMaskMembr[]  = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 12, 0, 0.0 } } };` |
|       - | 1527 | `	static const PH7_NativeAttrArg aMaskCallee[] = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 6,  0, 0.0 } } };` |
|       - | 1528 | `	static const PH7_NativeAttrArg aMaskAll[]    = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 127,0, 0.0 } } };` |
|       - | 1529 | `	static const struct {` |
|       - | 1530 | `		const char *zClass;` |
|       - | 1531 | `		const PH7_NativeAttrArg *aArg;   /* php's TARGET_* mask for that class */` |
|       - | 1532 | `	} aOwnAttr[] = {` |
|       - | 1533 | `		{ "Attribute",              aMaskClass  },   /* TARGET_CLASS */` |
|       - | 1534 | `		{ "Deprecated",             aMaskDep    },   /* CLASS\|FUNCTION\|METHOD\|CLASS_CONSTANT\|CONSTANT */` |
|       - | 1535 | `		{ "AllowDynamicProperties", aMaskClass  },   /* TARGET_CLASS */` |
|       - | 1536 | `		{ "SensitiveParameter",     aMaskParam  },   /* TARGET_PARAMETER */` |
|       - | 1537 | `		{ "ReturnTypeWillChange",   aMaskMethod },   /* TARGET_METHOD */` |
|       - | 1538 | `		{ "Override",               aMaskMembr  },   /* METHOD\|PROPERTY (php 8.5) */` |
|       - | 1539 | `		{ "NoDiscard",              aMaskCallee },   /* FUNCTION\|METHOD (php 8.5) */` |
|       - | 1540 | `		{ "DelayedTargetValidation", aMaskAll   },   /* TARGET_ALL (php 8.5) */` |
|       - | 1541 | `	};` |
|    6726 | 1542 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       - | 1543 | `	sxu32 n;` |
|   60494 | 1544 | `	for( n = 0 ; rc == SXRET_OK && n < SX_ARRAYSIZE(aOwnAttr) ; ++n ){` |
|   80621 | 1545 | `		rc = PH7_NativeClassAddAttribute(&(*pVm),` |
|   80616 | 1546 | `			PH7_VmExtractClass(&(*pVm),aOwnAttr[n].zClass,` |
|   53768 | 1547 | `				(sxu32)SyStrlen(aOwnAttr[n].zClass),FALSE,0),` |
|   53768 | 1548 | `			"Attribute",aOwnAttr[n].aArg,1);` |
|   26853 | 1549 | `	}` |
|    6726 | 1550 | `	return rc;` |
|       5 | 1551 | `}` |
|       - | 1552 | `/*` |
|       - | 1553 | ` * stdClass and Random\RandomException.` |
|       - | 1554 | ` *` |
|       - | 1555 | ` * stdClass is EMPTY in php too — it holds only dynamic properties — so the whole` |
|       - | 1556 | `` * declaration is the row. `Random\RandomException` is the first NAMESPACED class`` |
|       - | 1557 | ` * declared from C: the engine keys its class table by the FULLY QUALIFIED name` |
|       - | 1558 | `` * (the compiler resolves `namespace Random { class RandomException }` to exactly`` |
|       - | 1559 | ` * this string before installing), so a spec row spells the FQN and needs no` |
|       - | 1560 | ` * namespace machinery at all. It also retires the chunk this file kept ALONE for` |
|       - | 1561 | `` * it, whose comment explains why: a `namespace` declaration is not reset at its`` |
|       - | 1562 | ` * closing brace here, so anything following it in the same chunk would have` |
|       - | 1563 | ` * leaked into the Random namespace.` |
|       - | 1564 | ` */` |
|    6721 | 1565 | `static sxi32 VmInstallStdClasses(ph7_vm *pVm)` |
|       5 | 1566 | `{` |
|       - | 1567 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1568 | `		{ "stdClass", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1569 | `		/* unserialize()'s carrier for a disallowed or unknown class: as empty as` |
|       - | 1570 | `		 * stdClass (its properties are the payload's, created dynamically); what` |
|       - | 1571 | `		 * makes it special is the pVm->pIncClass checks at the access sites. */` |
|       - | 1572 | `		{ "__PHP_Incomplete_Class", 0, 0, PH7_CLASS_FINAL, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1573 | `		{ "Random\\RandomException", "Exception", 0, PH7_CLASS_NOCLONE,` |
|       - | 1574 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1575 | `		/* php 8.5's filter exceptions: FILTER_THROW_ON_FAILURE raises the second,` |
|       - | 1576 | `		 * and the first is the base a caller catches to mean "any filter error". */` |
|       - | 1577 | `		{ "Filter\\FilterException", "Exception", 0, 0,` |
|       - | 1578 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1579 | `		{ "Filter\\FilterFailedException", "Filter\\FilterException", 0, 0,` |
|       - | 1580 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1581 | `	};` |
|    6726 | 1582 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       5 | 1583 | `}` |
|    6721 | 1584 | `PH7_PRIVATE sxi32 PH7_VmInstallBuiltinLib(ph7_vm *pVm)` |
|       5 | 1585 | `{` |
|       - | 1586 | `	SyString sBuiltin;` |
|       - | 1587 | `	/* The interfaces first: everything below implements one of them` |
|       - | 1588 | `	 * (Exception implements Throwable). */` |
|    6726 | 1589 | `	VmInstallCoreInterfaces(&(*pVm));` |
|    6726 | 1590 | `	VmInstallExceptions(&(*pVm));` |
|    6726 | 1591 | `	VmInstallStdClasses(&(*pVm));` |
|    6726 | 1592 | `	VmInstallDirectory(&(*pVm));` |
|    6726 | 1593 | `	VmInstallAttributes(&(*pVm));` |
|    6726 | 1594 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|       - | 1595 | `	/* Compile the built-in library */` |
|    6726 | 1596 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|    6726 | 1597 | `	return SXRET_OK;` |
|       5 | 1598 | `}` |
|       - | 1599 |  |
