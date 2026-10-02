# src/ph7/vm_builtin_lib.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 460/526 lines (87.45%)

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
|       - |   83 | ``	"   directory reader php does not route through a stream wrapper, so `zzz://*` is"\`` |
|       - |   84 | ``	"   the directory named `zzz:` and the empty component after it: glob(3) lists"\`` |
|       - |   85 | ``	"   ./zzz:/ and keeps the pattern's slashes in every answer, `zzz://hit.php`."\`` |
|       - |   86 | `	"   PHL globbed through the wrapper lookup opendir() uses, so glob('file://'.'/tmp/*')"\` |
|       - |   87 | ``	"   listed /tmp where php answers [] (there is no directory `file:`), and later"\`` |
|       - |   88 | `	"   answered [] for ANY scheme, where a directory that really carries that name"\` |
|       - |   89 | `	"   is listed. Glob the one-slash spelling, which no wrapper lookup answers for,"\` |
|       - |   90 | `	"   and put the pattern's own slashes back. */"\` |
|       - |   91 | `	"if( preg_match('#^([A-Za-z][A-Za-z0-9+.\\\\-]*:/)(/+)(.*)$#s', $pattern, $aScheme) ){"\` |
|       - |   92 | `	"  $pArray = array();"\` |
|       - |   93 | `	"  foreach( glob($aScheme[1] . $aScheme[3], $flags) as $zHit ){"\` |
|       - |   94 | `	"    $pArray[] = $aScheme[1] . $aScheme[2] . substr($zHit, strlen($aScheme[1]));"\` |
|       - |   95 | `	"  }"\` |
|       - |   96 | `	"  return $pArray;"\` |
|       - |   97 | `	"}"\` |
|       - |   98 | `	"/* php's Z_PARAM_PATH refusal (see scandir above). It precedes the flag check:"\` |
|       - |   99 | `	"   php's ZPP runs before the function body. Without it the NUL was simply the end"\` |
|       - |  100 | `	"   of the pattern and glob() answered for the truncated one. */"\` |
|       - |  101 | `	"if( strpos($pattern, chr(0)) !== false ){"\` |
|       - |  102 | `	"  throw new ValueError('glob(): Argument #1 ($pattern) must not contain any null bytes');"\` |
|       - |  103 | `	"}"\` |
|       - |  104 | `	"/* php rejects a mask holding any bit outside GLOB_AVAILABLE_FLAGS with a warning"\` |
|       - |  105 | `	"   and FALSE. PHL accepted anything and just tested the bits it knew, so a stale"\` |
|       - |  106 | `	"   script passing the OLD PHL glob values (1/2/4/...) silently got a plain glob. */"\` |
|       - |  107 | `	"if( $flags & ~GLOB_AVAILABLE_FLAGS ){"\` |
|       - |  108 | `	"  trigger_error('glob(): At least one of the passed flags is invalid or not supported on this platform', E_USER_WARNING);"\` |
|       - |  109 | `	"  return FALSE;"\` |
|       - |  110 | `	"}"\` |
|       - |  111 | `	"/* GLOB_BRACE: expand the FIRST top-level {a,b,...} group and glob each"\` |
|       - |  112 | `	"   alternative IN ORDER, concatenating the answers (each sub-glob sorts its"\` |
|       - |  113 | `	"   own results; php never re-sorts across alternatives). Nested groups are"\` |
|       - |  114 | `	"   handled by the recursion, and GLOB_NOCHECK applies per EXPANDED pattern,"\` |
|       - |  115 | `	"   which is php's answer too. The flag used to be accepted and IGNORED, so"\` |
|       - |  116 | `	"   any braced pattern answered [] in silence. */"\` |
|       - |  117 | `	"if( $flags & GLOB_BRACE ){"\` |
|       - |  118 | `	"  $nLen = strlen($pattern); $iOpen = -1; $iClose = -1; $iDepth = 0;"\` |
|       - |  119 | `	"  for( $i = 0 ; $i < $nLen ; $i++ ){"\` |
|       - |  120 | `	"    $ch = $pattern[$i];"\` |
|       - |  121 | `	"    if( $ch === '{' ){ if( $iDepth === 0 ){ $iOpen = $i; } $iDepth++; }"\` |
|       - |  122 | `	"    else if( $ch === '}' && $iDepth > 0 ){ $iDepth--; if( $iDepth === 0 ){ $iClose = $i; break; } }"\` |
|       - |  123 | `	"  }"\` |
|       - |  124 | `	"  if( $iOpen >= 0 && $iClose > $iOpen ){"\` |
|       - |  125 | `	"    $zHead = substr($pattern,0,$iOpen);"\` |
|       - |  126 | `	"    $zBody = (string)substr($pattern,$iOpen+1,$iClose-$iOpen-1);"\` |
|       - |  127 | `	"    $zTail = (string)substr($pattern,$iClose+1);"\` |
|       - |  128 | `	"    $aAlt = array(); $zCur = ''; $iDepth = 0;"\` |
|       - |  129 | `	"    for( $i = 0 ; $i < strlen($zBody) ; $i++ ){"\` |
|       - |  130 | `	"      $ch = $zBody[$i];"\` |
|       - |  131 | `	"      if( $ch === '{' ){ $iDepth++; }"\` |
|       - |  132 | `	"      else if( $ch === '}' ){ $iDepth--; }"\` |
|       - |  133 | `	"      if( $ch === ',' && $iDepth === 0 ){ $aAlt[] = $zCur; $zCur = ''; continue; }"\` |
|       - |  134 | `	"      $zCur .= $ch;"\` |
|       - |  135 | `	"    }"\` |
|       - |  136 | `	"    $aAlt[] = $zCur;"\` |
|       - |  137 | `	"    $pArray = array();"\` |
|       - |  138 | `	"    foreach( $aAlt as $zAlt ){"\` |
|       - |  139 | `	"      $aSub = glob($zHead . $zAlt . $zTail,$flags);"\` |
|       - |  140 | `	"      if( $aSub !== false ){ foreach( $aSub as $zHit ){ $pArray[] = $zHit; } }"\` |
|       - |  141 | `	"    }"\` |
|       - |  142 | `	"    return $pArray;"\` |
|       - |  143 | `	"  }"\` |
|       - |  144 | `	"}"\` |
|       - |  145 | `	"/* A pattern that ENDS in a slash names DIRECTORIES, and php keeps the slash:"\` |
|       - |  146 | `	"   glob('d/') is ['d/'] and glob('d/*' . '/') is ['d/a/','d/b/']. Answer the base"\` |
|       - |  147 | `	"   without it, as directories, and put it back -- ONE slash at a time, so a"\` |
|       - |  148 | `	"   pattern ending in two keeps both. */"\` |
|       - |  149 | `	"if( substr($pattern,-1) === '/' ){"\` |
|       - |  150 | `	"  $zBase = substr($pattern,0,-1);"\` |
|       - |  151 | `	"  if( $zBase === '' ){"\` |
|       - |  152 | `	"    /* the pattern was '/' itself */"\` |
|       - |  153 | `	"    $pArray = is_dir('/') ? array('/') : array();"\` |
|       - |  154 | `	"  }else{"\` |
|       - |  155 | `	"    $pArray = array();"\` |
|       - |  156 | `	"    foreach( glob($zBase,($flags & ~(GLOB_MARK\|GLOB_NOCHECK)) \| GLOB_ONLYDIR) as $zHit ){"\` |
|       - |  157 | `	"      $pArray[] = $zHit . '/';"\` |
|       - |  158 | `	"    }"\` |
|       - |  159 | ``	"    /* php sorts the names it ANSWERS, slash included, so `a/../` comes before"\`` |
|       - |  160 | ``	"       `a/./` -- sorting the bases and appending afterwards has them the other"\`` |
|       - |  161 | `	"       way round. */"\` |
|       - |  162 | `	"    if( ($flags & GLOB_NOSORT) == 0 ){ sort($pArray,SORT_STRING); }"\` |
|       - |  163 | `	"  }"\` |
|       - |  164 | `	"  if( ($flags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){ $pArray[] = $pattern; }"\` |
|       - |  165 | `	"  return $pArray;"\` |
|       - |  166 | `	"}"\` |
|       - |  167 | `	"/* A wildcard in the DIRECTORY part is matched LEVEL BY LEVEL, which is what"\` |
|       - |  168 | `	"   glob(3) does: list the directories that part names, then glob the last"\` |
|       - |  169 | `	"   component inside each. Reading only the last component -- all this used to"\` |
|       - |  170 | ``	"   do -- answered [] for `src/*' . '/*.php', the everyday two-level spelling,"\`` |
|       - |  171 | `	"   and for every deeper one. */"\` |
|       - |  172 | `	"$slash = strrpos($pattern,'/');"\` |
|       - |  173 | `	"if( $slash !== false ){"\` |
|       - |  174 | `	"  $zHead = substr($pattern,0,$slash);"\` |
|       - |  175 | `	"  if( $zHead !== '' && strcspn($zHead,'*?[') != strlen($zHead) ){"\` |
|       - |  176 | `	"    $pArray = array();"\` |
|       - |  177 | `	"    foreach( glob($zHead . '/') as $zDirHit ){"\` |
|       - |  178 | `	"      foreach( glob($zDirHit . substr($pattern,$slash+1),$flags & ~GLOB_NOCHECK) as $zHit ){"\` |
|       - |  179 | `	"        $pArray[] = $zHit;"\` |
|       - |  180 | `	"      }"\` |
|       - |  181 | `	"    }"\` |
|       - |  182 | `	"    if( ($flags & GLOB_NOSORT) == 0 ){ sort($pArray,SORT_STRING); }"\` |
|       - |  183 | `	"    if( ($flags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){ $pArray[] = $pattern; }"\` |
|       - |  184 | `	"    return $pArray;"\` |
|       - |  185 | `	"  }"\` |
|       - |  186 | `	"}"\` |
|       - |  187 | `	"/* php keeps the literal directory portion of the pattern in every result;"\` |
|       - |  188 | `	"   split off everything up to and including the last '/' as the prefix. */"\` |
|       - |  189 | `	"$slash = strrpos($pattern,'/');"\` |
|       - |  190 | `	"if( $slash === false ){ $zDir = '.'; $prefix = ''; $pat = $pattern; }"\` |
|       - |  191 | `	"else { $zDir = substr($pattern,0,$slash); if( $zDir === '' ){ $zDir = '/'; } $prefix = substr($pattern,0,$slash+1); $pat = substr($pattern,$slash+1); }"\` |
|       - |  192 | `	"$pArray = array(); /* Empty array */"\` |
|       - |  193 | `	"/* php answers [] in SILENCE for a directory that cannot be opened — a"\` |
|       - |  194 | `	"   nonexistent path is simply zero matches (GLOB_ERR included; that flag is"\` |
|       - |  195 | `	"   about errors during the walk, not about the path). PHL used to let"\` |
|       - |  196 | `	"   opendir() warn and answered FALSE. The stat comes first because glob(3)"\` |
|       - |  197 | `	"   raises NO diagnostic at all: a suppressed opendir() still reaches an error"\` |
|       - |  198 | `	"   handler that ignores error_reporting(), and php's glob has none to reach. */"\` |
|       - |  199 | `	"$pHandle = is_dir($zDir) ? @opendir($zDir) : FALSE;"\` |
|       - |  200 | `	"if( $pHandle != FALSE ){"\` |
|       - |  201 | `	"/* Loop throw available entries */"\` |
|       - |  202 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|       - |  203 | `	" /* php's glob() never matches a leading-dot entry (incl. '.' and '..') unless"\` |
|       - |  204 | `	"    the pattern itself starts with a dot */"\` |
|       - |  205 | `	"	if( strlen($pEntry) > 0 && $pEntry[0] === '.' && (strlen($pat) < 1 \|\| $pat[0] !== '.') ){ continue; }"\` |
|       - |  206 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|       - |  207 | `	"	$rc = strglob($pat,$pEntry);"\` |
|       - |  208 | `	"	if( $rc ){"\` |
|       - |  209 | `	"	   $zFull = $prefix . $pEntry;"\` |
|       - |  210 | `	"	   if( is_dir($zDir . '/' . $pEntry) ){"\` |
|       - |  211 | `	"	      if( $flags & GLOB_MARK ){"\` |
|       - |  212 | `	"		     /* Adds a slash to each directory returned */"\` |
|       - |  213 | `	"			 $zFull .= DIRECTORY_SEPARATOR;"\` |
|       - |  214 | `	"		  }"\` |
|       - |  215 | `	"	   }else if( $flags & GLOB_ONLYDIR ){"\` |
|       - |  216 | `	"	     /* Not a directory,ignore */"\` |
|       - |  217 | `	"		 continue;"\` |
|       - |  218 | `	"	   }"\` |
|       - |  219 | `	"	   /* Add the entry (with its literal directory prefix, php-style) */"\` |
|       - |  220 | `	"	   $pArray[] = $zFull;"\` |
|       - |  221 | `	"	}"\` |
|       - |  222 | `	" }"\` |
|       - |  223 | `	"/* Close the handle */"\` |
|       - |  224 | `	"closedir($pHandle);"\` |
|       - |  225 | `	"}"\` |
|       - |  226 | `	"if( ($flags & GLOB_NOSORT) == 0 ){"\` |
|       - |  227 | `	"  /* glob(3) sorts with strcoll(), a BYTE compare in the C locale php runs in,"\` |
|       - |  228 | `	"     and it sorts the whole ANSWER rather than each directory it walked. The"\` |
|       - |  229 | `	"     default SORT_REGULAR compared numeric names as NUMBERS, so a directory of"\` |
|       - |  230 | ``	"     `1.jpg`..`10.jpg` came back in a different order than php lists it. */"\`` |
|       - |  231 | `	"  sort($pArray,SORT_STRING);"\` |
|       - |  232 | `	"}"\` |
|       - |  233 | `	"if( ($flags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|       - |  234 | `	"  /* Return the search pattern if no files matching were found */"\` |
|       - |  235 | `	"  $pArray[] = $pattern;"\` |
|       - |  236 | `	"}"\` |
|       - |  237 | `	"/* Return the created array */"\` |
|       - |  238 | `	"return $pArray;"\` |
|       - |  239 | `   "}"\` |
|       - |  240 | `   "/* Creates a temporary file */"\` |
|       - |  241 | `   "function tmpfile(){"\` |
|       - |  242 | `   "  /* Extract the temp directory */"\` |
|       - |  243 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|       - |  244 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|       - |  245 | `   "    /* Use the current dir */"\` |
|       - |  246 | `   "    $zTempDir = '.';"\` |
|       - |  247 | `   "  }"\` |
|       - |  248 | `   "  /* Create the file */"\` |
|       - |  249 | `   "  $zPath = $zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12);"\` |
|       - |  250 | `   "  /* php CREATES the file and then opens it r+b, which is the mode"\` |
|       - |  251 | `   "   * stream_get_meta_data() reports back for it. */"\` |
|       - |  252 | `   "  fclose(fopen($zPath,'w'));"\` |
|       - |  253 | `   "  $pHandle = fopen($zPath,'r+b');"\` |
|       - |  254 | `   "  return $pHandle;"\` |
|       - |  255 | `   "}"\` |
|       - |  256 | `   /* A builtin written here is INTERNAL to php, and php declares every one of` |
|       - |  257 | `    * these parameters. The declaration is not decoration: it is the ZPP screen,` |
|       - |  258 | `    * so an untyped $num CAST what php refuses -- is_nan('abc') answered false` |
|       - |  259 | `    * where php raises a TypeError, and each of the manual casts these bodies` |
|       - |  260 | `    * opened with was hiding exactly that. The return types are php's too; they` |
|       - |  261 | `    * are what ReflectionFunction prints. */\` |
|       - |  262 | `   "function is_nan(float $num): bool { return $num != $num; }"\` |
|       - |  263 | `   "function is_infinite(float $num): bool { return $num == INF \|\| $num == -INF; }"\` |
|       - |  264 | `   "function is_finite(float $num): bool { return !is_nan($num) && !is_infinite($num); }"\` |
|       - |  265 | `   "/* Inverse of bin2hex() */"\` |
|       - |  266 | `   "function hex2bin(string $string): string\|false {"\` |
|       - |  267 | `   "  $len = strlen($string);"\` |
|       - |  268 | `   "  if( $len % 2 !== 0 ){"\` |
|       - |  269 | `   "    trigger_error('hex2bin(): Hexadecimal input string must have an even length', E_USER_WARNING);"\` |
|       - |  270 | `   "    return false;"\` |
|       - |  271 | `   "  }"\` |
|       - |  272 | `   "  $out = '';"\` |
|       - |  273 | `   "  for( $i = 0 ; $i < $len ; $i += 2 ){"\` |
|       - |  274 | `   "    $pair = substr($string, $i, 2);"\` |
|       - |  275 | `   "    if( !ctype_xdigit($pair) ){"\` |
|       - |  276 | `   "      trigger_error('hex2bin(): Input string must be hexadecimal string', E_USER_WARNING);"\` |
|       - |  277 | `   "      return false;"\` |
|       - |  278 | `   "    }"\` |
|       - |  279 | `   "    $out = $out . chr(hexdec($pair));"\` |
|       - |  280 | `   "  }"\` |
|       - |  281 | `   "  return $out;"\` |
|       - |  282 | `   "}"\` |
|       - |  283 | ``   "/* Division that never throws: INF/-INF/NAN like php. The two `float`"\`` |
|       - |  284 | `   " * declarations are php's own: they are what refuses a non-numeric string"\` |
|       - |  285 | `   " * (an untyped $num1 cast to 0.0 and DIVIDED, so fdiv('abc',2) answered"\` |
|       - |  286 | `   " * float(0)), and what ReflectionFunction prints. */"\` |
|       - |  287 | `   "function fdiv(float $num1, float $num2): float {"\` |
|       - |  288 | `   "  if( $num2 == 0.0 ){"\` |
|       - |  289 | `   "    if( $num1 == 0.0 \|\| is_nan($num1) ){ return NAN; }"\` |
|       - |  290 | `   "    return $num1 > 0 ? INF : -INF;"\` |
|       - |  291 | `   "  }"\` |
|       - |  292 | `   "  return $num1 / $num2;"\` |
|       - |  293 | `   "}"\` |
|       - |  294 | `   "function checkdate(int $month, int $day, int $year): bool {"\` |
|       - |  295 | `   "  if( $month < 1 \|\| $month > 12 \|\| $year < 1 \|\| $year > 32767 \|\| $day < 1 ){ return false; }"\` |
|       - |  296 | `   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\` |
|       - |  297 | `   "  $max = $days[$month - 1];"\` |
|       - |  298 | `   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) \|\| ($year % 400 === 0)) ){"\` |
|       - |  299 | `   "    $max = 29;"\` |
|       - |  300 | `   "  }"\` |
|       - |  301 | `   "  return $day <= $max;"\` |
|       - |  302 | `   "}"\` |
|       - |  303 | `   "function is_iterable(mixed $value): bool { return is_array($value) \|\| ($value instanceof Traversable); }"\` |
|       - |  304 | `   "function is_countable(mixed $value): bool { return is_array($value) \|\| ($value instanceof Countable); }"\` |
|       - |  305 | `   "function doubleval(mixed $value): float { return (float)$value; }"\` |
|       - |  306 | `   "function array_count_values(array $array): array {"\` |
|       - |  307 | `   "  $out = array();"\` |
|       - |  308 | `   "  foreach( $array as $v ){"\` |
|       - |  309 | `   "    if( !is_int($v) && !is_string($v) ){"\` |
|       - |  310 | `   "      trigger_error('array_count_values(): Can only count string and integer values, entry skipped', E_USER_WARNING);"\` |
|       - |  311 | `   "      continue;"\` |
|       - |  312 | `   "    }"\` |
|       - |  313 | `   "    if( isset($out[$v]) ){ $out[$v] = $out[$v] + 1; } else { $out[$v] = 1; }"\` |
|       - |  314 | `   "  }"\` |
|       - |  315 | `   "  return $out;"\` |
|       - |  316 | `   "}"\` |
|       - |  317 | `   "function array_change_key_case(array $array, int $case = CASE_LOWER): array {"\` |
|       - |  318 | `   "  $out = array();"\` |
|       - |  319 | `   "  foreach( $array as $k => $v ){"\` |
|       - |  320 | `   "    if( is_string($k) ){ $k = ($case == CASE_UPPER) ? strtoupper($k) : strtolower($k); }"\` |
|       - |  321 | `   "    $out[$k] = $v;"\` |
|       - |  322 | `   "  }"\` |
|       - |  323 | `   "  return $out;"\` |
|       - |  324 | `   "}"\` |
|       - |  325 | `   "function array_replace_recursive(array $array, array ...$replacements): array {"\` |
|       - |  326 | `   "  foreach( $replacements as $o ){"\` |
|       - |  327 | `   "    foreach( $o as $k => $v ){"\` |
|       - |  328 | `   "      if( is_array($v) && isset($array[$k]) && is_array($array[$k]) ){"\` |
|       - |  329 | `   "        $array[$k] = array_replace_recursive($array[$k], $v);"\` |
|       - |  330 | `   "      }else{"\` |
|       - |  331 | `   "        $array[$k] = $v;"\` |
|       - |  332 | `   "      }"\` |
|       - |  333 | `   "    }"\` |
|       - |  334 | `   "  }"\` |
|       - |  335 | `   "  return $array;"\` |
|       - |  336 | `   "}"\` |
|       - |  337 | `   /* class_parents/class_implements/class_uses moved to C (vm_builtin_class.c):` |
|       - |  338 | `    * as prelude wrappers they gated on class_exists(), so an interface, a trait` |
|       - |  339 | `    * and an enum all answered FALSE where php answers a list; class_uses could` |
|       - |  340 | `    * not reach the trait table at all and returned the empty set for every class;` |
|       - |  341 | `    * and the E_WARNING php raises for a name nothing declares cannot be raised` |
|       - |  342 | `    * from here at php's severity or against the CALLER's line. */\` |
|       - |  343 | `   "function ip2long(string $ip): int\|false {"\` |
|       - |  344 | `   "  $p = explode('.', $ip);"\` |
|       - |  345 | `   "  if( count($p) !== 4 ){ return false; }"\` |
|       - |  346 | `   "  $n = 0;"\` |
|       - |  347 | `   "  foreach( $p as $o ){"\` |
|       - |  348 | `   "    if( !ctype_digit($o) \|\| (int)$o < 0 \|\| (int)$o > 255 ){ return false; }"\` |
|       - |  349 | `   "    $n = $n * 256 + (int)$o;"\` |
|       - |  350 | `   "  }"\` |
|       - |  351 | `   "  return $n;"\` |
|       - |  352 | `   "}"\` |
|       - |  353 | `   "function long2ip(int $ip): string {"\` |
|       - |  354 | `   "  return (($ip >> 24) & 255) . '.' . (($ip >> 16) & 255) . '.' . (($ip >> 8) & 255) . '.' . ($ip & 255);"\` |
|       - |  355 | `   "}"\` |
|       - |  356 | `   "/* php 8.3 str_increment(): Perl-style alphanumeric increment. */"\` |
|       - |  357 | `   "function str_increment(string $string): string {"\` |
|       - |  358 | `   "  if( $string === '' ){ throw new ValueError('str_increment(): Argument #1 ($string) must not be empty'); }"\` |
|       - |  359 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_increment(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|       - |  360 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|       - |  361 | `   "    $c = $string[$i];"\` |
|       - |  362 | `   "    if( $c === 'z' ){ $string[$i] = 'a'; }"\` |
|       - |  363 | `   "    elseif( $c === 'Z' ){ $string[$i] = 'A'; }"\` |
|       - |  364 | `   "    elseif( $c === '9' ){ $string[$i] = '0'; }"\` |
|       - |  365 | `   "    else { $string[$i] = chr(ord($c) + 1); return $string; }"\` |
|       - |  366 | `   "  }"\` |
|       - |  367 | `   "  $first = $string[0];"\` |
|       - |  368 | `   "  if( $first === '0' ){ return '1' . $string; }"\` |
|       - |  369 | `   "  if( $first === 'a' ){ return 'a' . $string; }"\` |
|       - |  370 | `   "  return 'A' . $string;"\` |
|       - |  371 | `   "}"\` |
|       - |  372 | `   "/* php 8.3 str_decrement(): inverse of str_increment(); throws out of range"\` |
|       - |  373 | `   " * at the bottom of the counting sequence. */"\` |
|       - |  374 | `   "function str_decrement(string $string): string {"\` |
|       - |  375 | `   "  if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) must not be empty'); }"\` |
|       - |  376 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_decrement(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|       - |  377 | `   "  $orig = $string;"\` |
|       - |  378 | `   "  $borrowed = false;"\` |
|       - |  379 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|       - |  380 | `   "    $c = $string[$i];"\` |
|       - |  381 | `   "    if( $c === 'a' ){ $string[$i] = 'z'; }"\` |
|       - |  382 | `   "    elseif( $c === 'A' ){ $string[$i] = 'Z'; }"\` |
|       - |  383 | `   "    elseif( $c === '0' ){ $string[$i] = '9'; }"\` |
|       - |  384 | `   "    else { $string[$i] = chr(ord($c) - 1); $borrowed = false; break; }"\` |
|       - |  385 | `   "    if( $i === 0 ){ $borrowed = true; }"\` |
|       - |  386 | `   "  }"\` |
|       - |  387 | `   "  if( $borrowed ){"\` |
|       - |  388 | `   "    if( $string[0] === '9' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|       - |  389 | `   "    $string = substr($string, 1);"\` |
|       - |  390 | `   "    if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|       - |  391 | `   "  } elseif( strlen($string) > 1 && $string[0] === '0' ){"\` |
|       - |  392 | `   "    $string = substr($string, 1);"\` |
|       - |  393 | `   "  }"\` |
|       - |  394 | `   "  return $string;"\` |
|       - |  395 | `   "}"\` |
|       - |  396 | `   /* fileperms/fileowner/filegroup/fileinode moved to C (vfs.c, VfsStatField):` |
|       - |  397 | `    * as prelude wrappers over stat() three of them said nothing on a failed stat` |
|       - |  398 | `    * and the fourth raised trigger_error, whose errno is E_USER_WARNING's 512 and` |
|       - |  399 | `    * whose line is this chunk's rather than the caller's. */\` |
|       - |  400 | `   "/* PH7 keeps no stat cache, so this is a no-op like php on a clean cache. */"\` |
|       - |  401 | `   "function clearstatcache(bool $clear_realpath_cache = false, string $filename = ''): void {}"\` |
|       - |  402 | `   /* mb_ucfirst/mb_lcfirst moved to C (builtin_mb.c): as prelude wrappers they` |
|       - |  403 | `    * dropped $encoding, UPPER-cased where php title-cases ('ß' -> 'SS' for php's` |
|       - |  404 | `    * 'Ss') and lowered a leading Σ with nothing after it, which is php's FINAL` |
|       - |  405 | `    * sigma and not what a first character gets. */\` |
|       - |  406 | `   "/* Creates a temporary file and returns its name */"\` |
|       - |  407 | `   "function tempnam(string $directory,string $prefix): string\|false"\` |
|       - |  408 | `   "{"\` |
|       - |  409 | `   "   /* php's Z_PARAM_PATH refusal on BOTH parameters (see scandir above); the prefix"\` |
|       - |  410 | `   "    * is a path fragment there too, and PHL used to build a filename with the NUL"\` |
|       - |  411 | `   "    * still in it. */"\` |
|       - |  412 | `   "   if( strpos($directory, chr(0)) !== false ){"\` |
|       - |  413 | `   "     throw new ValueError('tempnam(): Argument #1 ($directory) must not contain any null bytes');"\` |
|       - |  414 | `   "   }"\` |
|       - |  415 | `   "   if( strpos($prefix, chr(0)) !== false ){"\` |
|       - |  416 | `   "     throw new ValueError('tempnam(): Argument #2 ($prefix) must not contain any null bytes');"\` |
|       - |  417 | `   "   }"\` |
|       - |  418 | `   "   /* php falls back to the system temporary directory when the one it was"\` |
|       - |  419 | `   "    * given cannot HOLD the file, and says so -- except for the empty"\` |
|       - |  420 | ``   "    * directory, which it reads as `use the temp dir` and answers silently."\`` |
|       - |  421 | `   "    * PHL took '' literally and spent 64 tries failing at the filesystem"\` |
|       - |  422 | `   "    * ROOT. Whether a directory can hold it is settled by TRYING, not by"\` |
|       - |  423 | `   "    * asking is_writable(): the two disagree on Windows. */"\` |
|       - |  424 | `   "   $zTmp = rtrim(sys_get_temp_dir(), DIRECTORY_SEPARATOR);"\` |
|       - |  425 | `   "   $zDir = $directory === '' ? $zTmp : rtrim($directory, DIRECTORY_SEPARATOR);"\` |
|       - |  426 | `   "   if( is_dir($zDir) && is_writable($zDir) ){"\` |
|       - |  427 | `   "     $zOut = __tempnam_in($zDir, $prefix);"\` |
|       - |  428 | `   "     if( $zOut !== false ){ return $zOut; }"\` |
|       - |  429 | `   "   }"\` |
|       - |  430 | `   "   if( $zDir === $zTmp ){ return false; }"\` |
|       - |  431 | `   "   trigger_error(\"tempnam(): file created in the system's temporary directory\", E_USER_NOTICE);"\` |
|       - |  432 | `   "   return __tempnam_in($zTmp, $prefix);"\` |
|       - |  433 | `   "}"\` |
|       - |  434 | `   "function __tempnam_in(string $zDir, string $prefix)"\` |
|       - |  435 | `   "{"\` |
|       - |  436 | `   "   /* php CREATES the file (empty, mode 0600) and guarantees the name is"\` |
|       - |  437 | `   "    * unique -- returning a bare name left the caller with a path that does"\` |
|       - |  438 | `   "    * not exist, so file_exists() was false and unlink() failed on it. */"\` |
|       - |  439 | `   "   for( $i = 0 ; $i < 64 ; ++$i ){"\` |
|       - |  440 | `   "     $zPath = $zDir.DIRECTORY_SEPARATOR.$prefix.rand_str(12);"\` |
|       - |  441 | `   "     if( file_exists($zPath) ){ continue; }"\` |
|       - |  442 | `   "     $pHandle = @fopen($zPath,'x');"\` |
|       - |  443 | `   "     if( $pHandle === false ){ return false; }"\` |
|       - |  444 | `   "     fclose($pHandle);"\` |
|       - |  445 | `   "     @chmod($zPath, 0600);"\` |
|       - |  446 | `   "     return $zPath;"\` |
|       - |  447 | `   "   }"\` |
|       - |  448 | `   "   return false;"\` |
|       - |  449 | `   "}"\` |
|       - |  450 | `	/* fileowner/filegroup/fileinode: see the note beside fileperms above. */\` |
|       - |  451 | `	""` |
|       - |  452 |  |
|       - |  453 | `/*` |
|       - |  454 | ` * ---------------------------------------------------------------------------` |
|       - |  455 | ` * The Exception / Error family, declared from C.` |
|       - |  456 | ` *` |
|       - |  457 | ` * php's two roots are one implementation twice over (its stub says` |
|       - |  458 | `` * `@implementation-alias Exception::__construct` for every one of Error's`` |
|       - |  459 | ` * methods), so the bodies below are shared by both spec tables and the` |
|       - |  460 | ` * ~20 subclasses are declaration-only rows.` |
|       - |  461 | ` *` |
|       - |  462 | `` * php's seven slots, in php's own declaration order. `string` is php's cache of`` |
|       - |  463 | ` * the __toString rendering -- unused by the engine but PRESENT on every` |
|       - |  464 | ` * presentation surface, which is why it is declared here rather than skipped:` |
|       - |  465 | ` * var_dump/print_r/(array)/serialize all show it, and PHL was one property short` |
|       - |  466 | ` * of php on every exception ever printed.` |
|       - |  467 | ` * ---------------------------------------------------------------------------` |
|       - |  468 | ` */` |
|       - |  469 | `#define EXC_MESSAGE  "message"` |
|       - |  470 | `#define EXC_STRING   "string"` |
|       - |  471 | `#define EXC_CODE     "code"` |
|       - |  472 | `#define EXC_FILE     "file"` |
|       - |  473 | `#define EXC_LINE     "line"` |
|       - |  474 | `#define EXC_TRACE    "trace"` |
|       - |  475 | `#define EXC_PREVIOUS "previous"` |
|       - |  476 | `#define EXC_SEVERITY "severity"` |
|       - |  477 | `/*` |
|       - |  478 | ` * Answer a declared slot the way php's getter does. Three of the seven CONVERT` |
|       - |  479 | ` * rather than copy — getMessage()/getFile() answer a string and getLine() an int,` |
|       - |  480 | ` * whatever the slot holds — and that shows twice: a subclass assigning` |
|       - |  481 | `` * `$this->message = 5` reads back "5", and a slot __wakeup has DROPPED reads as`` |
|       - |  482 | ` * "" rather than null. The other four are verbatim copies (getCode() of that same` |
|       - |  483 | ` * subclass really is the int).` |
|       - |  484 | ` */` |
|       - |  485 | `#define EXC_READ_RAW 0` |
|       - |  486 | `#define EXC_READ_STR 1` |
|       - |  487 | `#define EXC_READ_INT 2` |
|   18435 |  488 | `static int VmExcReadSlot(ph7_context *pCtx,const char *zSlot,int iAs)` |
|       5 |  489 | `{` |
|   18440 |  490 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   18440 |  491 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,zSlot) : 0;` |
|       - |  492 | `	ph7_value sTmp;` |
|   18440 |  493 | `	if( iAs == EXC_READ_RAW ){` |
|     584 |  494 | `		if( pVal ){` |
|     584 |  495 | `			ph7_result_value(pCtx,pVal);` |
|     294 |  496 | `		}else{` |
|     ! 0 |  497 | `			ph7_result_null(pCtx);` |
|       - |  498 | `		}` |
|     584 |  499 | `		return PH7_OK;` |
|       - |  500 | `	}` |
|       - |  501 | `	/* Through a COPY: converting the slot would rewrite the exception's state. */` |
|   17860 |  502 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|   17860 |  503 | `	if( pVal ){` |
|   17860 |  504 | `		PH7_MemObjStore(pVal,&sTmp);` |
|    8900 |  505 | `	}` |
|   17860 |  506 | `	if( iAs == EXC_READ_INT ){` |
|     711 |  507 | `		PH7_MemObjToInteger(&sTmp);` |
|     358 |  508 | `	}else{` |
|   17154 |  509 | `		PH7_MemObjToString(&sTmp);` |
|       - |  510 | `	}` |
|   17860 |  511 | `	ph7_result_value(pCtx,&sTmp);` |
|   17860 |  512 | `	PH7_MemObjRelease(&sTmp);` |
|   17860 |  513 | `	return PH7_OK;` |
|    9195 |  514 | `}` |
|   16479 |  515 | `static int vm_builtin_Exception_getMessage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  516 | `{` |
|    8212 |  517 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   16484 |  518 | `	return VmExcReadSlot(pCtx,EXC_MESSAGE,EXC_READ_STR);` |
|       5 |  519 | `}` |
|     494 |  520 | `static int vm_builtin_Exception_getCode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  521 | `{` |
|     247 |  522 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     498 |  523 | `	return VmExcReadSlot(pCtx,EXC_CODE,EXC_READ_RAW);` |
|       4 |  524 | `}` |
|     670 |  525 | `static int vm_builtin_Exception_getFile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  526 | `{` |
|     335 |  527 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     675 |  528 | `	return VmExcReadSlot(pCtx,EXC_FILE,EXC_READ_STR);` |
|       5 |  529 | `}` |
|     706 |  530 | `static int vm_builtin_Exception_getLine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  531 | `{` |
|     353 |  532 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     711 |  533 | `	return VmExcReadSlot(pCtx,EXC_LINE,EXC_READ_INT);` |
|       5 |  534 | `}` |
|      60 |  535 | `static int vm_builtin_Exception_getTrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  536 | `{` |
|      30 |  537 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      62 |  538 | `	return VmExcReadSlot(pCtx,EXC_TRACE,EXC_READ_RAW);` |
|       2 |  539 | `}` |
|      22 |  540 | `static int vm_builtin_Exception_getPrevious(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  541 | `{` |
|      11 |  542 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      24 |  543 | `	return VmExcReadSlot(pCtx,EXC_PREVIOUS,EXC_READ_RAW);` |
|       2 |  544 | `}` |
|       4 |  545 | `static int vm_builtin_ErrorException_getSeverity(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  546 | `{` |
|       2 |  547 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       5 |  548 | `	return VmExcReadSlot(pCtx,EXC_SEVERITY,EXC_READ_RAW);` |
|       1 |  549 | `}` |
|       - |  550 | `/*` |
|       - |  551 | ` * php's zend_update_exception_properties: each of the three is written only when` |
|       - |  552 | ``  * the caller actually supplied it — a message when the argument was PASSED (`""` `` |
|       - |  553 | ` * included), a code when it is NON-ZERO, a previous when it is an object. That is` |
|       - |  554 | ` * not the same as writing the defaults: a subclass may redeclare` |
|       - |  555 | `` * `protected $message = 'default'`, and php keeps it for `new Sub()`.`` |
|       - |  556 | ` */` |
| 1466127 |  557 | `static void VmExcInitProps(ph7_context *pCtx,ph7_class_instance *pThis,int nArg,` |
|       - |  558 | `	ph7_value **apArg,int iPrev)` |
|       5 |  559 | `{` |
| 1466132 |  560 | `	if( nArg > 0 ){` |
| 1466028 |  561 | `		int nMsg = 0;` |
| 1466028 |  562 | `		const char *zMsg = ph7_value_to_string(apArg[0],&nMsg);` |
| 1466028 |  563 | `		PH7_NativeSetAttrStr(pCtx->pVm,pThis,EXC_MESSAGE,zMsg,nMsg);` |
|  732984 |  564 | `	}` |
| 1466132 |  565 | `	if( nArg > 1 ){` |
|       - |  566 | `		ph7_value sCode;` |
|     517 |  567 | `		PH7_MemObjInit(pCtx->pVm,&sCode);` |
|     517 |  568 | `		PH7_MemObjStore(apArg[1],&sCode);` |
|     517 |  569 | `		PH7_MemObjToInteger(&sCode);` |
|     517 |  570 | `		if( sCode.x.iVal != 0 ){` |
|     488 |  571 | `			PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_CODE,sCode.x.iVal);` |
|     243 |  572 | `		}` |
|     517 |  573 | `		PH7_MemObjRelease(&sCode);` |
|     256 |  574 | `	}` |
|       - |  575 | ``	/* php's `previous` is the LAST parameter of each constructor, and`` |
|       - |  576 | `	 * ErrorException's is #5 rather than #2. */` |
| 1466132 |  577 | `	if( nArg > iPrev && (apArg[iPrev]->iFlags & MEMOBJ_OBJ) && apArg[iPrev]->x.pOther ){` |
|      34 |  578 | `		PH7_NativeSetAttrObj(pCtx->pVm,pThis,EXC_PREVIOUS,` |
|      20 |  579 | `			(ph7_class_instance *)apArg[iPrev]->x.pOther);` |
|      10 |  580 | `	}` |
| 1466132 |  581 | `}` |
| 1466111 |  582 | `static int vm_builtin_Exception_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  583 | `{` |
| 1466116 |  584 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
| 1466116 |  585 | `	if( pThis ){` |
| 1466116 |  586 | `		VmExcInitProps(pCtx,pThis,nArg,apArg,2);` |
|  733028 |  587 | `	}` |
| 1466116 |  588 | `	return PH7_OK;` |
|       5 |  589 | `}` |
|       - |  590 | `/*` |
|       - |  591 | ` * ErrorException's own constructor: php's Exception three, then severity, then` |
|       - |  592 | `` * the OPTIONAL file/line overrides. php's `?string $filename = null` /`` |
|       - |  593 | `` * `?int $line = null` mean "keep the creation site" — the chunk defaulted them to`` |
|       - |  594 | ` * __FILE__/__LINE__, which resolved against the EMBEDDED chunk and reported` |
|       - |  595 | `` * `:MEMORY:` line 1 for every ErrorException that did not pass them. php's one`` |
|       - |  596 | ` * asymmetry: a filename WITHOUT a line resets the line to 0.` |
|       - |  597 | ` */` |
|      16 |  598 | `static int vm_builtin_ErrorException_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  599 | `{` |
|      17 |  600 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      17 |  601 | `	if( pThis == 0 ){` |
|     ! 0 |  602 | `		return PH7_OK;` |
|       - |  603 | `	}` |
|      17 |  604 | `	VmExcInitProps(pCtx,pThis,nArg,apArg,5);` |
|      17 |  605 | `	if( nArg > 2 ){` |
|      11 |  606 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_SEVERITY,ph7_value_to_int64(apArg[2]));` |
|       5 |  607 | `	}` |
|      17 |  608 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|       9 |  609 | `		int nFile = 0;` |
|       9 |  610 | `		const char *zFile = ph7_value_to_string(apArg[3],&nFile);` |
|       9 |  611 | `		PH7_NativeSetAttrStr(pCtx->pVm,pThis,EXC_FILE,zFile,nFile);` |
|       9 |  612 | `		if( nArg < 5 \|\| ph7_value_is_null(apArg[4]) ){` |
|       3 |  613 | `			PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_LINE,0);` |
|       1 |  614 | `		}` |
|       4 |  615 | `	}` |
|      17 |  616 | `	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|       7 |  617 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_LINE,ph7_value_to_int64(apArg[4]));` |
|       3 |  618 | `	}` |
|      17 |  619 | `	return PH7_OK;` |
|       9 |  620 | `}` |
|       - |  621 | `/*` |
|       - |  622 | ` * php's private __clone. It has an empty body and is never reached: the class` |
|       - |  623 | ` * carries php's own clone refusal (PH7_CLASS_NOCLONE, answered before any body` |
|       - |  624 | `` * runs), which is what `clone $e` reports — "Trying to clone an uncloneable`` |
|       - |  625 | ` * object of class X", not a visibility error. Declaring it is still php-visible:` |
|       - |  626 | `` * Reflection lists it, and `$e->__clone()` from inside the class works.`` |
|       - |  627 | ` */` |
|     ! 0 |  628 | `static int vm_builtin_Exception_clone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  629 | `{` |
|     ! 0 |  630 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  631 | `	ph7_result_null(pCtx);` |
|     ! 0 |  632 | `	return PH7_OK;` |
|     ! 0 |  633 | `}` |
|       - |  634 | `/*` |
|       - |  635 | ` * php's __wakeup: the two UNTYPED slots are the only ones a serialized payload` |
|       - |  636 | ` * can lie about (the other five are typed and the store enforces them), so php` |
|       - |  637 | ` * DROPS a message that is not a string and a code that is not an int rather than` |
|       - |  638 | ` * letting a method read one.` |
|       - |  639 | ` */` |
|     ! 0 |  640 | `static void VmExcDropSlot(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot)` |
|     ! 0 |  641 | `{` |
|     ! 0 |  642 | `	SyHashEntry *pEntry = SyHashGet(&pThis->hAttr,(const void *)zSlot,SyStrlen(zSlot));` |
|     ! 0 |  643 | `	if( pEntry ){` |
|     ! 0 |  644 | `		PH7_VmReleaseInstanceAttr(&(*pVm),(VmClassAttr *)pEntry->pUserData);` |
|     ! 0 |  645 | `		PH7_ClassInstanceDeleteAttrEntry(pThis,pEntry);` |
|     ! 0 |  646 | `	}` |
|     ! 0 |  647 | `}` |
|     ! 0 |  648 | `static int vm_builtin_Exception_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  649 | `{` |
|     ! 0 |  650 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - |  651 | `	ph7_value *pVal;` |
|     ! 0 |  652 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  653 | `	if( pThis == 0 ){` |
|     ! 0 |  654 | `		return PH7_OK;` |
|       - |  655 | `	}` |
|     ! 0 |  656 | `	pVal = PH7_NativeAttr(pThis,EXC_MESSAGE);` |
|     ! 0 |  657 | `	if( pVal && (pVal->iFlags & MEMOBJ_NULL) == 0 && (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 |  658 | `		VmExcDropSlot(pCtx->pVm,pThis,EXC_MESSAGE);` |
|     ! 0 |  659 | `	}` |
|     ! 0 |  660 | `	pVal = PH7_NativeAttr(pThis,EXC_CODE);` |
|     ! 0 |  661 | `	if( pVal && (pVal->iFlags & MEMOBJ_NULL) == 0 && (pVal->iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 |  662 | `		VmExcDropSlot(pCtx->pVm,pThis,EXC_CODE);` |
|     ! 0 |  663 | `	}` |
|     ! 0 |  664 | `	ph7_result_null(pCtx);` |
|     ! 0 |  665 | `	return PH7_OK;` |
|     ! 0 |  666 | `}` |
|       - |  667 | `/*` |
|       - |  668 | ` * One argument of a trace frame, php's smart_str_append_scalar: a string is` |
|       - |  669 | `` * single-quoted, ESCAPED (`\n`, `\xNN` for anything non-printable) and truncated`` |
|       - |  670 | `` * to 15 bytes with `...` inside the quotes; a float takes php's precision; an`` |
|       - |  671 | `` * enum case prints `Enum::Case`; and anything else is a bare word.`` |
|       - |  672 | ` */` |
|       - |  673 | `#define EXC_ARG_MAX 15` |
|     106 |  674 | `static void VmExcTraceArg(ph7_vm *pVm,SyBlob *pOut,ph7_value *pArg)` |
|       2 |  675 | `{` |
|     108 |  676 | `	if( pArg == 0 \|\| (pArg->iFlags & MEMOBJ_NULL) ){` |
|       3 |  677 | `		SyBlobAppend(pOut,"NULL",sizeof("NULL")-1);` |
|       3 |  678 | `		return;` |
|       - |  679 | `	}` |
|     106 |  680 | `	if( pArg->iFlags & MEMOBJ_BOOL ){` |
|       5 |  681 | `		if( pArg->x.iVal ){` |
|       3 |  682 | `			SyBlobAppend(pOut,"true",sizeof("true")-1);` |
|       2 |  683 | `		}else{` |
|       3 |  684 | `			SyBlobAppend(pOut,"false",sizeof("false")-1);` |
|       - |  685 | `		}` |
|       5 |  686 | `		return;` |
|       - |  687 | `	}` |
|     102 |  688 | `	if( pArg->iFlags & MEMOBJ_HASHMAP ){` |
|       3 |  689 | `		SyBlobAppend(pOut,"Array",sizeof("Array")-1);` |
|       3 |  690 | `		return;` |
|       - |  691 | `	}` |
|     100 |  692 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|       3 |  693 | `		ph7_class_instance *pObj = (ph7_class_instance *)pArg->x.pOther;` |
|       3 |  694 | `		if( pObj && pObj->pClass && (pObj->pClass->iFlags & PH7_CLASS_ENUM) ){` |
|     ! 0 |  695 | `			ph7_value *pName = PH7_NativeAttr(pObj,"name");` |
|     ! 0 |  696 | `			SyBlobFormat(pOut,"%z::",&pObj->pClass->sDisp);` |
|     ! 0 |  697 | `			if( pName ){` |
|     ! 0 |  698 | `				SyBlobAppend(pOut,SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|     ! 0 |  699 | `			}` |
|     ! 0 |  700 | `			return;` |
|       - |  701 | `		}` |
|       3 |  702 | `		SyBlobAppend(pOut,"Object(",sizeof("Object(")-1);` |
|       3 |  703 | `		if( pObj && pObj->pClass ){` |
|       3 |  704 | `			SyBlobFormat(pOut,"%z",&pObj->pClass->sDisp);` |
|       1 |  705 | `		}` |
|       3 |  706 | `		SyBlobAppend(pOut,")",sizeof(")")-1);` |
|       3 |  707 | `		return;` |
|       - |  708 | `	}` |
|      98 |  709 | `	if( pArg->iFlags & MEMOBJ_STRING ){` |
|       - |  710 | `		/* php 8.5 does not put string CONTENT in a trace at all: every non-empty` |
|       - |  711 | `		 * one renders as '...' (the empty one still shows as ''), so a password` |
|       - |  712 | `		 * or a token passed to the function that threw cannot reach a log through` |
|       - |  713 | `		 * the trace. The truncate-at-15-and-escape shape here was php 8.4's. */` |
|      52 |  714 | `		if( SyBlobLength(&pArg->sBlob) < 1 ){` |
|       3 |  715 | `			SyBlobAppend(pOut,"''",sizeof("''")-1);` |
|       2 |  716 | `		}else{` |
|      50 |  717 | `			SyBlobAppend(pOut,"'...'",sizeof("'...'")-1);` |
|       - |  718 | `		}` |
|      52 |  719 | `		return;` |
|       - |  720 | `	}` |
|       - |  721 | `	{` |
|       - |  722 | `		/* int / float / anything else: php prints the scalar itself -- but a` |
|       - |  723 | `		 * trace FLOAT always shows its fraction (1.0, not the "1" the ordinary` |
|       - |  724 | `		 * string cast produces), which is what tells a float argument apart from` |
|       - |  725 | `		 * an int one. INF/NAN and the exponent forms already carry a marker. */` |
|       - |  726 | `		ph7_value sTmp;` |
|       - |  727 | `		const char *z;` |
|       - |  728 | `		sxu32 n,i;` |
|      47 |  729 | `		int bMarked = 0;` |
|      47 |  730 | `		PH7_MemObjInit(&(*pVm),&sTmp);` |
|      47 |  731 | `		PH7_MemObjStore(pArg,&sTmp);` |
|      47 |  732 | `		PH7_MemObjToString(&sTmp);` |
|      47 |  733 | `		z = (const char *)SyBlobData(&sTmp.sBlob);` |
|      47 |  734 | `		n = SyBlobLength(&sTmp.sBlob);` |
|      47 |  735 | `		SyBlobAppend(pOut,z,n);` |
|      47 |  736 | `		if( pArg->iFlags & MEMOBJ_REAL ){` |
|      23 |  737 | `			for( i = 0 ; i < n ; ++i ){` |
|      19 |  738 | `				if( z[i] < '0' \|\| z[i] > '9' ){` |
|      11 |  739 | `					if( z[i] != '-' && z[i] != '+' ){` |
|       9 |  740 | `						bMarked = 1;` |
|       9 |  741 | `						break;` |
|       - |  742 | `					}` |
|       1 |  743 | `				}` |
|       6 |  744 | `			}` |
|      13 |  745 | `			if( !bMarked ){` |
|       5 |  746 | `				SyBlobAppend(pOut,".0",sizeof(".0")-1);` |
|       2 |  747 | `			}` |
|       6 |  748 | `		}` |
|      47 |  749 | `		PH7_MemObjRelease(&sTmp);` |
|       - |  750 | `	}` |
|      55 |  751 | `}` |
|       - |  752 | `/* An element of a trace frame, or NULL when the frame does not carry it. */` |
|    4158 |  753 | `static ph7_value * VmExcFrameField(ph7_vm *pVm,ph7_hashmap *pFrame,const char *zField)` |
|       5 |  754 | `{` |
|    4163 |  755 | `	ph7_hashmap_node *pNode = 0;` |
|       - |  756 | `	ph7_value sKey;` |
|       - |  757 | `	sxi32 rc;` |
|       - |  758 | `	SyString sName;` |
|    4163 |  759 | `	SyStringInitFromBuf(&sName,zField,SyStrlen(zField));` |
|    4163 |  760 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,&sName);` |
|    4163 |  761 | `	rc = PH7_HashmapLookup(pFrame,&sKey,&pNode);` |
|    4163 |  762 | `	PH7_MemObjRelease(&sKey);` |
|    4163 |  763 | `	if( rc != SXRET_OK \|\| pNode == 0 ){` |
|    1953 |  764 | `		return 0;` |
|       - |  765 | `	}` |
|    2215 |  766 | `	return (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|    2084 |  767 | `}` |
|    2776 |  768 | `static void VmExcFrameStr(SyBlob *pOut,ph7_value *pVal)` |
|       5 |  769 | `{` |
|    2781 |  770 | `	if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){` |
|    1441 |  771 | `		SyBlobAppend(pOut,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|     718 |  772 | `	}` |
|    2781 |  773 | `}` |
|       - |  774 | `/* A slot's string form, taken through a COPY: converting the value in place` |
|       - |  775 | ` * would rewrite the exception's own state. */` |
|       6 |  776 | `static void VmExcValueStr(ph7_vm *pVm,ph7_value *pVal,SyBlob *pOut)` |
|       1 |  777 | `{` |
|       - |  778 | `	ph7_value sTmp;` |
|       7 |  779 | `	if( pVal == 0 ){` |
|     ! 0 |  780 | `		return;` |
|       - |  781 | `	}` |
|       7 |  782 | `	PH7_MemObjInit(&(*pVm),&sTmp);` |
|       7 |  783 | `	PH7_MemObjStore(pVal,&sTmp);` |
|       7 |  784 | `	PH7_MemObjToString(&sTmp);` |
|       7 |  785 | `	SyBlobAppend(pOut,SyBlobData(&sTmp.sBlob),SyBlobLength(&sTmp.sBlob));` |
|       7 |  786 | `	PH7_MemObjRelease(&sTmp);` |
|       4 |  787 | `}` |
|       - |  788 | ``/* Does the blob contain this literal? SyBlobSearch() is `#ifndef`` |
|       - |  789 | `` * PH7_DISABLE_BUILTIN_FUNC`, and the exception family exists in the tiny build`` |
|       - |  790 | ` * too, so the one search this file needs is spelled out. */` |
|     ! 0 |  791 | `static int VmExcBlobHas(SyBlob *pBlob,const char *zPat,sxu32 nPat)` |
|     ! 0 |  792 | `{` |
|     ! 0 |  793 | `	const char *z = (const char *)SyBlobData(pBlob);` |
|     ! 0 |  794 | `	sxu32 n = SyBlobLength(pBlob);` |
|       - |  795 | `	sxu32 i;` |
|     ! 0 |  796 | `	if( nPat == 0 \|\| n < nPat ){` |
|     ! 0 |  797 | `		return 0;` |
|       - |  798 | `	}` |
|     ! 0 |  799 | `	for( i = 0 ; i + nPat <= n ; i++ ){` |
|     ! 0 |  800 | `		if( SyMemcmp((const void *)&z[i],(const void *)zPat,nPat) == 0 ){` |
|     ! 0 |  801 | `			return 1;` |
|       - |  802 | `		}` |
|     ! 0 |  803 | `	}` |
|     ! 0 |  804 | `	return 0;` |
|     ! 0 |  805 | `}` |
|       - |  806 | ``/* php's `Z_OBJCE_P == zend_ce_type_error \|\| == zend_ce_argument_count_error`:`` |
|       - |  807 | ` * the two classes whose message __toString finishes with " and defined". */` |
|       6 |  808 | `static int VmExcIsArgError(ph7_vm *pVm,ph7_class_instance *pExc)` |
|       1 |  809 | `{` |
|       7 |  810 | `	ph7_class *pClass = pExc ? pExc->pClass : 0;` |
|       - |  811 | `	ph7_class *pType;` |
|       7 |  812 | `	if( pClass == 0 ){` |
|     ! 0 |  813 | `		return 0;` |
|       - |  814 | `	}` |
|       7 |  815 | `	pType = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,FALSE,0);` |
|       7 |  816 | `	if( pType && pClass == pType ){` |
|     ! 0 |  817 | `		return 1;` |
|       - |  818 | `	}` |
|       7 |  819 | `	pType = PH7_VmExtractClass(&(*pVm),"ArgumentCountError",sizeof("ArgumentCountError")-1,FALSE,0);` |
|       7 |  820 | `	return pType != 0 && pClass == pType;` |
|       4 |  821 | `}` |
|       - |  822 | `/*` |
|       - |  823 | `` * php's zend_trace_to_string: one `#N file(line): Class->method(args)` line per`` |
|       - |  824 | `` * frame, then `#N {main}` with NO trailing newline. A frame with no `file` is`` |
|       - |  825 | `` * php's `[internal function]: `.`` |
|       - |  826 | ` */` |
|     712 |  827 | `PH7_PRIVATE void PH7_VmTraceToString(ph7_vm *pVm,ph7_value *pTrace,int bMainMarker,SyBlob *pOut)` |
|       5 |  828 | `{` |
|       - |  829 | `	ph7_hashmap *pMap;` |
|       - |  830 | `	ph7_hashmap_node *pEntry;` |
|     717 |  831 | `	sxu32 nFrame = 0;` |
|     717 |  832 | `	if( pTrace && (pTrace->iFlags & MEMOBJ_HASHMAP) && pTrace->x.pOther ){` |
|     717 |  833 | `		pMap = (ph7_hashmap *)pTrace->x.pOther;` |
|       - |  834 | `		/* Insertion order is pFirst then the pPrev chain (rule 12). */` |
|    1411 |  835 | `		for( pEntry = pMap->pFirst ; pEntry ; pEntry = pEntry->pPrev ){` |
|     699 |  836 | `			ph7_value *pFrameVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pEntry->nValIdx);` |
|       - |  837 | `			ph7_hashmap *pFrame;` |
|       - |  838 | `			ph7_value *pFile;` |
|     699 |  839 | `			if( pFrameVal == 0 \|\| (pFrameVal->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 |  840 | `				continue;` |
|       - |  841 | `			}` |
|     699 |  842 | `			pFrame = (ph7_hashmap *)pFrameVal->x.pOther;` |
|     699 |  843 | `			SyBlobFormat(pOut,"#%u ",nFrame);` |
|     699 |  844 | `			pFile = VmExcFrameField(&(*pVm),pFrame,"file");` |
|    1043 |  845 | `			if( pFile && (pFile->iFlags & MEMOBJ_STRING) ){` |
|     693 |  846 | `				ph7_value *pLine = VmExcFrameField(&(*pVm),pFrame,"line");` |
|     693 |  847 | `				VmExcFrameStr(pOut,pFile);` |
|    1381 |  848 | `				SyBlobFormat(pOut,"(%qd): ",` |
|     688 |  849 | `					(pLine && (pLine->iFlags & MEMOBJ_INT)) ? pLine->x.iVal : (sxi64)0);` |
|     349 |  850 | `			}else{` |
|       8 |  851 | `				SyBlobAppend(pOut,"[internal function]: ",sizeof("[internal function]: ")-1);` |
|       - |  852 | `			}` |
|     699 |  853 | `			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"class"));` |
|     699 |  854 | `			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"type"));` |
|     699 |  855 | `			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"function"));` |
|     699 |  856 | `			SyBlobAppend(pOut,"(",sizeof("(")-1);` |
|       - |  857 | `			{` |
|     699 |  858 | `				ph7_value *pArgs = VmExcFrameField(&(*pVm),pFrame,"args");` |
|     699 |  859 | `				if( pArgs && (pArgs->iFlags & MEMOBJ_HASHMAP) && pArgs->x.pOther ){` |
|      94 |  860 | `					ph7_hashmap *pArgMap = (ph7_hashmap *)pArgs->x.pOther;` |
|       - |  861 | `					ph7_hashmap_node *pArg;` |
|      94 |  862 | `					int bFirst = 1;` |
|     200 |  863 | `					for( pArg = pArgMap->pFirst ; pArg ; pArg = pArg->pPrev ){` |
|     108 |  864 | `						if( !bFirst ){` |
|      17 |  865 | `							SyBlobAppend(pOut,", ",sizeof(", ")-1);` |
|       8 |  866 | `						}` |
|     108 |  867 | `						bFirst = 0;` |
|     161 |  868 | `						VmExcTraceArg(&(*pVm),pOut,` |
|      53 |  869 | `							(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pArg->nValIdx));` |
|      55 |  870 | `					}` |
|      46 |  871 | `				}` |
|       - |  872 | `			}` |
|     699 |  873 | `			SyBlobAppend(pOut,")\n",sizeof(")\n")-1);` |
|     699 |  874 | `			nFrame++;` |
|     352 |  875 | `		}` |
|     356 |  876 | `	}` |
|     717 |  877 | `	if( bMainMarker ){` |
|       - |  878 | `		/* getTraceAsString() ends on the bottom marker; debug_print_backtrace()` |
|       - |  879 | `		 * does not print one -- it stops after the last real frame. */` |
|     675 |  880 | `		SyBlobFormat(pOut,"#%u {main}",nFrame);` |
|     335 |  881 | `	}` |
|     717 |  882 | `}` |
|     642 |  883 | `static int vm_builtin_Exception_getTraceAsString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  884 | `{` |
|     647 |  885 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - |  886 | `	SyBlob sOut;` |
|     321 |  887 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     647 |  888 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     647 |  889 | `	PH7_VmTraceToString(pCtx->pVm,pThis ? PH7_NativeAttr(pThis,EXC_TRACE) : 0,TRUE,&sOut);` |
|     647 |  890 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     647 |  891 | `	SyBlobRelease(&sOut);` |
|     647 |  892 | `	return PH7_OK;` |
|       5 |  893 | `}` |
|       - |  894 | `/*` |
|       - |  895 | ` * php's Exception::__toString.` |
|       - |  896 | ` *` |
|       - |  897 | ` *    C: message in file:line` |
|       - |  898 | ` *    Stack trace:` |
|       - |  899 | ` *    <trace>` |
|       - |  900 | ` *` |
|       - |  901 | ` * The PREVIOUS chain is part of the format and the ORDER is inverted: php builds` |
|       - |  902 | `` * the string innermost-first and joins the shallower ones after `\n\nNext `, so`` |
|       - |  903 | ` * the root cause is printed first. The chunk answered a four-field space-joined` |
|       - |  904 | `` * line instead — `file line code message` — which no php ever produced, and it is`` |
|       - |  905 | `` * what an uncaught exception, `echo $e` and `(string)$e` all show.`` |
|       - |  906 | ` *` |
|       - |  907 | ` * The walk carries its ancestors on the C stack (rule 31): php protects each` |
|       - |  908 | `` * object it visits and stops when it comes back round, and a `$a->previous = $b;`` |
|       - |  909 | `` * $b->previous = $a` pair must not spin.`` |
|       - |  910 | ` */` |
|       - |  911 | `#define EXC_CHAIN_MAX 256` |
|       4 |  912 | `static int vm_builtin_Exception_toString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  913 | `{` |
|       - |  914 | `	ph7_class_instance *apChain[EXC_CHAIN_MAX];` |
|       5 |  915 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       5 |  916 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - |  917 | `	SyBlob sOut;` |
|       5 |  918 | `	int nChain = 0;` |
|       - |  919 | `	int i,j;` |
|       2 |  920 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      11 |  921 | `	while( pThis && nChain < EXC_CHAIN_MAX ){` |
|       - |  922 | `		ph7_class_instance *pPrev;` |
|       9 |  923 | `		for( j = 0 ; j < nChain ; j++ ){` |
|       3 |  924 | `			if( apChain[j] == pThis ){` |
|     ! 0 |  925 | `				pThis = 0;    /* already on the chain: php's recursion protection */` |
|     ! 0 |  926 | `				break;` |
|       - |  927 | `			}` |
|       2 |  928 | `		}` |
|       7 |  929 | `		if( pThis == 0 ){` |
|     ! 0 |  930 | `			break;` |
|       - |  931 | `		}` |
|       7 |  932 | `		apChain[nChain++] = pThis;` |
|       7 |  933 | `		pPrev = PH7_NativeAttrObj(pThis,EXC_PREVIOUS);` |
|       7 |  934 | `		pThis = pPrev;` |
|       1 |  935 | `	}` |
|       - |  936 | `	/* php formats the SHALLOWEST first and pushes each one it has already built` |
|       - |  937 | `	 * behind the next, so the printed order is inverted: the ROOT CAUSE leads and` |
|       - |  938 | ``	 * every caller follows it after `\n\nNext `. */`` |
|       5 |  939 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      11 |  940 | `	for( i = 0 ; i < nChain ; i++ ){` |
|       7 |  941 | `		ph7_class_instance *pExc = apChain[i];` |
|       7 |  942 | `		ph7_value *pLine = PH7_NativeAttr(pExc,EXC_LINE);` |
|       - |  943 | `		SyBlob sMsg;` |
|       - |  944 | `		SyBlob sThis;` |
|       7 |  945 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       7 |  946 | `		SyBlobInit(&sThis,&pVm->sAllocator);` |
|       7 |  947 | `		VmExcValueStr(pVm,PH7_NativeAttr(pExc,EXC_MESSAGE),&sMsg);` |
|       - |  948 | `		/* php's one message rewrite: a TypeError/ArgumentCountError raised at a` |
|       - |  949 | `		 * CALL SITE says "..., called in F on line N", and __toString finishes the` |
|       - |  950 | `		 * sentence with " and defined". */` |
|       6 |  951 | `		if( VmExcIsArgError(pVm,pExc)` |
|       4 |  952 | `		 && VmExcBlobHas(&sMsg,", called in ",sizeof(", called in ")-1) ){` |
|     ! 0 |  953 | `			SyBlobAppend(&sMsg," and defined",sizeof(" and defined")-1);` |
|     ! 0 |  954 | `		}` |
|       7 |  955 | `		SyBlobFormat(&sThis,"%z",&pExc->pClass->sDisp);` |
|       7 |  956 | `		if( SyBlobLength(&sMsg) > 0 ){` |
|       5 |  957 | `			SyBlobAppend(&sThis,": ",sizeof(": ")-1);` |
|       5 |  958 | `			SyBlobAppend(&sThis,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       2 |  959 | `		}` |
|       7 |  960 | `		SyBlobAppend(&sThis," in ",sizeof(" in ")-1);` |
|       7 |  961 | `		VmExcFrameStr(&sThis,PH7_NativeAttr(pExc,EXC_FILE));` |
|      10 |  962 | `		SyBlobFormat(&sThis,":%qd\nStack trace:\n",` |
|       6 |  963 | `			(pLine && (pLine->iFlags & MEMOBJ_INT)) ? pLine->x.iVal : (sxi64)0);` |
|       7 |  964 | `		PH7_VmTraceToString(pVm,PH7_NativeAttr(pExc,EXC_TRACE),TRUE,&sThis);` |
|       7 |  965 | `		if( SyBlobLength(&sOut) > 0 ){` |
|       3 |  966 | `			SyBlobAppend(&sThis,"\n\nNext ",sizeof("\n\nNext ")-1);` |
|       3 |  967 | `			SyBlobAppend(&sThis,SyBlobData(&sOut),SyBlobLength(&sOut));` |
|       1 |  968 | `		}` |
|       7 |  969 | `		SyBlobReset(&sOut);` |
|       7 |  970 | `		SyBlobAppend(&sOut,SyBlobData(&sThis),SyBlobLength(&sThis));` |
|       7 |  971 | `		SyBlobRelease(&sMsg);` |
|       7 |  972 | `		SyBlobRelease(&sThis);` |
|       4 |  973 | `	}` |
|       5 |  974 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|       5 |  975 | `	SyBlobRelease(&sOut);` |
|       5 |  976 | `	return PH7_OK;` |
|       1 |  977 | `}` |
|       - |  978 | `/*` |
|       - |  979 | ` * The declaration. php's two roots carry the same eleven methods and the same` |
|       - |  980 | `` * seven slots; the only difference php's stub records is `Error::$line`, which`` |
|       - |  981 | ` * has NO default where Exception's is 0.` |
|       - |  982 | ` *` |
|       - |  983 | `` * PH7_CLASS_NOCLONE on EVERY row: php refuses `clone $e` outright, and a native`` |
|       - |  984 | ` * subclass does not inherit its parent's class flags (rule 29).` |
|       - |  985 | ` */` |
|       - |  986 | `#define EXC_METHODS(zCtor,xCtor) \` |
|       - |  987 | `	{ "__clone",          PH7_MOD_PRIVATE, "", "void", vm_builtin_Exception_clone }, \` |
|       - |  988 | `	{ "__construct",      PH7_MOD_PUBLIC, zCtor, 0, xCtor }, \` |
|       - |  989 | `	{ "__wakeup",         PH7_MOD_PUBLIC, "", "@void", vm_builtin_Exception_wakeup }, \` |
|       - |  990 | `	{ "getMessage",       PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "string", \` |
|       - |  991 | `	  vm_builtin_Exception_getMessage }, \` |
|       - |  992 | `	{ "getCode",          PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", 0, \` |
|       - |  993 | `	  vm_builtin_Exception_getCode }, \` |
|       - |  994 | `	{ "getFile",          PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "string", \` |
|       - |  995 | `	  vm_builtin_Exception_getFile }, \` |
|       - |  996 | `	{ "getLine",          PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "int", \` |
|       - |  997 | `	  vm_builtin_Exception_getLine }, \` |
|       - |  998 | `	{ "getTrace",         PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "array", \` |
|       - |  999 | `	  vm_builtin_Exception_getTrace }, \` |
|       - | 1000 | `	{ "getPrevious",      PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "?Throwable", \` |
|       - | 1001 | `	  vm_builtin_Exception_getPrevious }, \` |
|       - | 1002 | `	{ "getTraceAsString", PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "string", \` |
|       - | 1003 | `	  vm_builtin_Exception_getTraceAsString }, \` |
|       - | 1004 | `	{ "__toString",       PH7_MOD_PUBLIC, "", "string", vm_builtin_Exception_toString }` |
|       - | 1005 | `#define EXC_CTOR_SIG "string $message = \"\", int $code = 0, ?Throwable $previous = null"` |
|       - | 1006 | `/* php's seven slots, twice: the only difference between the two roots is` |
|       - | 1007 | `` * `Error::$line`, which php's stub declares with NO default where Exception's is`` |
|       - | 1008 | `` * 0 (`PH7_NATIVE_VAL_NONE` — its hasDefaultValue() is false and the export`` |
|       - | 1009 | `` * prints `protected int $line` bare). `message` and `code` are the two php leaves`` |
|       - | 1010 | ` * UNTYPED, and its stub says why: BC, since a subclass may have assigned` |
|       - | 1011 | ` * anything to them. */` |
|       - | 1012 | `#define EXC_PROP_HEAD \` |
|       - | 1013 | `	{ EXC_MESSAGE,  PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 }, \` |
|       - | 1014 | `	{ EXC_STRING,   PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" }, \` |
|       - | 1015 | `	{ EXC_CODE,     PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 }, \` |
|       - | 1016 | `	{ EXC_FILE,     PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" }` |
|       - | 1017 | `#define EXC_PROP_TAIL \` |
|       - | 1018 | `	{ EXC_TRACE,    PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_ARRAY, 0, 0, 0.0 }, "array" }, \` |
|       - | 1019 | `	{ EXC_PREVIOUS, PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?Throwable" }` |
|    7925 | 1020 | `static sxi32 VmInstallExceptions(ph7_vm *pVm)` |
|       5 | 1021 | `{` |
|       - | 1022 | `	static const PH7_NativeMethodDef aExcMethod[] = {` |
|       - | 1023 | `		EXC_METHODS(EXC_CTOR_SIG,vm_builtin_Exception_construct)` |
|       - | 1024 | `	};` |
|       - | 1025 | `	static const PH7_NativePropDef aExcProp[] = {` |
|       - | 1026 | `		EXC_PROP_HEAD,` |
|       - | 1027 | `		{ EXC_LINE, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, "int" },` |
|       - | 1028 | `		EXC_PROP_TAIL` |
|       - | 1029 | `	};` |
|       - | 1030 | `	static const PH7_NativePropDef aErrProp[] = {` |
|       - | 1031 | `		EXC_PROP_HEAD,` |
|       - | 1032 | `		{ EXC_LINE, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|       - | 1033 | `		EXC_PROP_TAIL` |
|       - | 1034 | `	};` |
|       - | 1035 | `	static const PH7_NativePropDef aErrExcProp[] = {` |
|       - | 1036 | `		{ EXC_SEVERITY, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 1, 0, 0.0 }, "int" },` |
|       - | 1037 | `	};` |
|       - | 1038 | `	static const PH7_NativeMethodDef aErrExcMethod[] = {` |
|       - | 1039 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|       - | 1040 | `		  "string $message = \"\", int $code = 0, int $severity = E_ERROR, "` |
|       - | 1041 | `		  "?string $filename = null, ?int $line = null, ?Throwable $previous = null", 0,` |
|       - | 1042 | `		  vm_builtin_ErrorException_construct },` |
|       - | 1043 | `		{ "getSeverity", PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "int",` |
|       - | 1044 | `		  vm_builtin_ErrorException_getSeverity },` |
|       - | 1045 | `	};` |
|       - | 1046 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1047 | `		{ "Exception", 0, "Throwable", PH7_CLASS_NOCLONE,` |
|       - | 1048 | `		  aExcMethod, SX_ARRAYSIZE(aExcMethod), 0, 0, aExcProp, SX_ARRAYSIZE(aExcProp), 0, 0, 0 },` |
|       - | 1049 | `		{ "Error", 0, "Throwable", PH7_CLASS_NOCLONE,` |
|       - | 1050 | `		  aExcMethod, SX_ARRAYSIZE(aExcMethod), 0, 0, aErrProp, SX_ARRAYSIZE(aErrProp), 0, 0, 0 },` |
|       - | 1051 | `		/* Zend's own subclasses, then ErrorException, then SPL's tree. Every row is` |
|       - | 1052 | `		 * declaration-only in php too. */` |
|       - | 1053 | `		{ "TypeError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1054 | `		{ "ArgumentCountError", "TypeError", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1055 | `		{ "ValueError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1056 | `		{ "FiberError", "Error", 0,` |
|       - | 1057 | `		  PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1058 | `		{ "AssertionError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1059 | `		{ "ArithmeticError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1060 | `		{ "DivisionByZeroError", "ArithmeticError", 0, PH7_CLASS_NOCLONE,` |
|       - | 1061 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1062 | `		{ "UnhandledMatchError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1063 | `		{ "CompileError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1064 | `		{ "ParseError", "CompileError", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1065 | `		{ "ErrorException", "Exception", 0, PH7_CLASS_NOCLONE,` |
|       - | 1066 | `		  aErrExcMethod, SX_ARRAYSIZE(aErrExcMethod), 0, 0,` |
|       - | 1067 | `		  aErrExcProp, SX_ARRAYSIZE(aErrExcProp), 0, 0, 0 },` |
|       - | 1068 | `		{ "LogicException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1069 | `		{ "RuntimeException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1070 | `		{ "BadFunctionCallException", "LogicException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1071 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1072 | `		{ "BadMethodCallException", "BadFunctionCallException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1073 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1074 | `		{ "DomainException", "LogicException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1075 | `		{ "InvalidArgumentException", "LogicException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1076 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1077 | `		{ "LengthException", "LogicException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1078 | `		{ "OutOfRangeException", "LogicException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1079 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1080 | `		{ "OutOfBoundsException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1081 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1082 | `		{ "OverflowException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1083 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1084 | `		{ "RangeException", "RuntimeException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1085 | `		{ "UnderflowException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1086 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1087 | `		{ "UnexpectedValueException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1088 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1089 | `		{ "JsonException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1090 | `	};` |
|       - | 1091 | `	{` |
|    7930 | 1092 | `		sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|    7930 | 1093 | `		if( rc == SXRET_OK ){` |
|       - | 1094 | ``			/* php refuses `new FiberError` -- the engine is the only thing that`` |
|       - | 1095 | `			 * raises one -- and words the refusal per class. */` |
|    7930 | 1096 | `			ph7_class *pFe = PH7_VmExtractClass(&(*pVm),"FiberError",` |
|       - | 1097 | `				sizeof("FiberError")-1,FALSE,0);` |
|    7930 | 1098 | `			if( pFe ){` |
|    7930 | 1099 | `				pFe->zNewRefusal = "The \"FiberError\" class is reserved for internal use "` |
|       - | 1100 | `					"and cannot be manually instantiated";` |
|    3957 | 1101 | `			}` |
|    3957 | 1102 | `		}` |
|    7930 | 1103 | `		return rc;` |
|       - | 1104 | `	}` |
|       5 | 1105 | `}` |
|       - | 1106 | `/*` |
|       - | 1107 | ` * The eleven core interfaces, declared from C.` |
|       - | 1108 | ` *` |
|       - | 1109 | ` * They are contracts -- no method here has a body, every row is` |
|       - | 1110 | ` * PH7_MOD_ABSTRACT -- so the conversion is entirely about what the DECLARATION` |
|       - | 1111 | ` * says, which is where a chunk fell short in four php-visible ways:` |
|       - | 1112 | ` *` |
|       - | 1113 | `` *  - php's `interface Throwable extends Stringable`: the chunk redeclared`` |
|       - | 1114 | ` *    __toString() on Throwable instead, so no Exception was ever Stringable` |
|       - | 1115 | `` *    (`$e instanceof Stringable` was false, and Reflection attributed the`` |
|       - | 1116 | `` *    method to Throwable rather than printing php's `inherits Stringable`);`` |
|       - | 1117 | ` *  - php declares a RETURN TYPE on all but three of these methods and marks` |
|       - | 1118 | `` *    nearly all of them TENTATIVE (the leading `@`, rule 45) -- a chunk has no`` |
|       - | 1119 | ` *    way to say tentative at all;` |
|       - | 1120 | `` *  - php's `mixed` on ArrayAccess's offsets, which the chunk left untyped;`` |
|       - | 1121 | ` *  - method ORDER, which Reflection prints: php lists Throwable's getPrevious` |
|       - | 1122 | ` *    before getTraceAsString, and Iterator's as current/next/key/valid/rewind.` |
|       - | 1123 | ` *` |
|       - | 1124 | ` * Order within the table is php's stub order too; the declare-then-link phases` |
|       - | 1125 | ` * of PH7_InstallNativeClasses let Throwable name Stringable and Iterator name` |
|       - | 1126 | `` * Traversable regardless of row order. An interface's parent is `zParent`, not`` |
|       - | 1127 | `` * `zImplements` (Reflection walks pBase to attribute an inherited method).`` |
|       - | 1128 | ` */` |
|    7925 | 1129 | `static sxi32 VmInstallCoreInterfaces(ph7_vm *pVm)` |
|       5 | 1130 | `{` |
|       - | 1131 | `	static const PH7_NativeMethodDef aStringable[] = {` |
|       - | 1132 | `		{ "__toString", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1133 | `	};` |
|       - | 1134 | `	static const PH7_NativeMethodDef aThrowable[] = {` |
|       - | 1135 | `		/* Not one of these is tentative: php's Throwable is a real contract. */` |
|       - | 1136 | `		{ "getMessage",       PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1137 | `		{ "getCode",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|       - | 1138 | `		{ "getFile",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1139 | `		{ "getLine",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "int", 0 },` |
|       - | 1140 | `		{ "getTrace",         PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "array", 0 },` |
|       - | 1141 | `		{ "getPrevious",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "?Throwable", 0 },` |
|       - | 1142 | `		{ "getTraceAsString", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1143 | `	};` |
|       - | 1144 | `	static const PH7_NativeMethodDef aArrayAccess[] = {` |
|       - | 1145 | `		{ "offsetExists", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset", "@bool", 0 },` |
|       - | 1146 | `		{ "offsetGet",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset", "@mixed", 0 },` |
|       - | 1147 | `		{ "offsetSet",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset, mixed $value",` |
|       - | 1148 | `		  "@void", 0 },` |
|       - | 1149 | `		{ "offsetUnset",  PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset", "@void", 0 },` |
|       - | 1150 | `	};` |
|       - | 1151 | `	static const PH7_NativeMethodDef aCountable[] = {` |
|       - | 1152 | `		{ "count", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@int", 0 },` |
|       - | 1153 | `	};` |
|       - | 1154 | `	static const PH7_NativeMethodDef aJsonSerializable[] = {` |
|       - | 1155 | `		{ "jsonSerialize", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@mixed", 0 },` |
|       - | 1156 | `	};` |
|       - | 1157 | `	/* The concrete cases()/from()/tryFrom() an enum gets are native methods` |
|       - | 1158 | `	 * declared to match these (oo_native.c, PH7_InstallEnumInterfaceMethods). */` |
|       - | 1159 | `	static const PH7_NativeMethodDef aUnitEnum[] = {` |
|       - | 1160 | `		{ "cases", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_ABSTRACT, "", "array", 0 },` |
|       - | 1161 | `	};` |
|       - | 1162 | `	static const PH7_NativeMethodDef aBackedEnum[] = {` |
|       - | 1163 | `		{ "from",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_ABSTRACT, "string\|int $value",` |
|       - | 1164 | `		  "static", 0 },` |
|       - | 1165 | `		{ "tryFrom", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_ABSTRACT, "string\|int $value",` |
|       - | 1166 | `		  "?static", 0 },` |
|       - | 1167 | `	};` |
|       - | 1168 | `	static const PH7_NativeMethodDef aIterator[] = {` |
|       - | 1169 | `		{ "current", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@mixed", 0 },` |
|       - | 1170 | `		{ "next",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@void", 0 },` |
|       - | 1171 | `		{ "key",     PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@mixed", 0 },` |
|       - | 1172 | `		{ "valid",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@bool", 0 },` |
|       - | 1173 | `		{ "rewind",  PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@void", 0 },` |
|       - | 1174 | `	};` |
|       - | 1175 | `	static const PH7_NativeMethodDef aIteratorAggregate[] = {` |
|       - | 1176 | `		{ "getIterator", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@Traversable", 0 },` |
|       - | 1177 | `	};` |
|       - | 1178 | `	/* php's legacy Serializable declares NO return type on either method. */` |
|       - | 1179 | `	static const PH7_NativeMethodDef aSerializable[] = {` |
|       - | 1180 | `		{ "serialize",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|       - | 1181 | `		{ "unserialize", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $data", 0, 0 },` |
|       - | 1182 | `	};` |
|       - | 1183 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1184 | `		{ "Traversable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1185 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1186 | `		{ "Stringable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1187 | `		  aStringable, SX_ARRAYSIZE(aStringable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1188 | `		{ "Throwable", "Stringable", 0, PH7_CLASS_INTERFACE,` |
|       - | 1189 | `		  aThrowable, SX_ARRAYSIZE(aThrowable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1190 | `		{ "ArrayAccess", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1191 | `		  aArrayAccess, SX_ARRAYSIZE(aArrayAccess), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1192 | `		{ "Countable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1193 | `		  aCountable, SX_ARRAYSIZE(aCountable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1194 | `		{ "JsonSerializable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1195 | `		  aJsonSerializable, SX_ARRAYSIZE(aJsonSerializable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1196 | `		{ "UnitEnum", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1197 | `		  aUnitEnum, SX_ARRAYSIZE(aUnitEnum), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1198 | `		{ "BackedEnum", "UnitEnum", 0, PH7_CLASS_INTERFACE,` |
|       - | 1199 | `		  aBackedEnum, SX_ARRAYSIZE(aBackedEnum), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1200 | `		{ "Iterator", "Traversable", 0, PH7_CLASS_INTERFACE,` |
|       - | 1201 | `		  aIterator, SX_ARRAYSIZE(aIterator), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1202 | `		{ "IteratorAggregate", "Traversable", 0, PH7_CLASS_INTERFACE,` |
|       - | 1203 | `		  aIteratorAggregate, SX_ARRAYSIZE(aIteratorAggregate), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1204 | `		{ "Serializable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1205 | `		  aSerializable, SX_ARRAYSIZE(aSerializable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1206 | `	};` |
|    7930 | 1207 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       5 | 1208 | `}` |
|       - | 1209 | `/*` |
|       - | 1210 | ` * ---------------------------------------------------------------------------` |
|       - | 1211 | `` * php's Directory — the object `dir()` answers.`` |
|       - | 1212 | ` *` |
|       - | 1213 | ` * php declares it FINAL with **no constructor at all**: the class is created by` |
|       - | 1214 | `` * `dir()` and `new Directory` is refused in the create_object handler, with a`` |
|       - | 1215 | ` * sentence that names dir() as the way to get one. Its two slots are` |
|       - | 1216 | `` * `public protected(set) readonly`, so a script can read `$d->path` and never`` |
|       - | 1217 | `` * write it, and its three methods declare return types (`read(): string\|false`).`` |
|       - | 1218 | ` * The chunk had a public constructor, a __destruct php does not declare, no` |
|       - | 1219 | ` * types anywhere and writable slots.` |
|       - | 1220 | ` * ---------------------------------------------------------------------------` |
|       - | 1221 | ` */` |
|       - | 1222 | `#define DIR_HANDLE "handle"` |
|       - | 1223 | `#define DIR_PATH   "path"` |
|       - | 1224 | `/*` |
|       - | 1225 | ` * Forward one method to the engine's own directory builtin (rule 7: call, don't` |
|       - | 1226 | `` * reimplement). php's Directory methods are `php_stream_readdir(...)` on the very`` |
|       - | 1227 | `` * stream `readdir()` uses, and a CLOSED handle is a TypeError there — the one`` |
|       - | 1228 | ` * place php's wording names the class rather than the function.` |
|       - | 1229 | ` */` |
|      40 | 1230 | `static int VmDirClosed(ph7_value *pHandle)` |
|       2 | 1231 | `{` |
|      42 | 1232 | `	io_private *pDev = (io_private *)pHandle->x.pOther;` |
|      42 | 1233 | `	return IO_PRIVATE_INVALID(pDev);` |
|       2 | 1234 | `}` |
|      40 | 1235 | `static int VmDirForward(ph7_context *pCtx,const char *zFunc,const char *zMethod)` |
|       2 | 1236 | `{` |
|      42 | 1237 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      42 | 1238 | `	ph7_value *pHandle = pThis ? PH7_NativeAttr(pThis,DIR_HANDLE) : 0;` |
|       - | 1239 | `	ph7_value *apArg[1];` |
|       - | 1240 | `	ph7_value sResult;` |
|       - | 1241 | `	ph7_value sName;` |
|       - | 1242 | `	SyString sStr;` |
|       - | 1243 | `	sxi32 rc;` |
|       - | 1244 | ``	/* php's check is `php_stream_from_zval` on a stream it CLOSED: closedir()`` |
|       - | 1245 | `	 * keeps the resource alive and marks it (gettype() answers` |
|       - | 1246 | `	 * "resource (closed)"), so the test is the magic, not the type. */` |
|      40 | 1247 | `	if( pHandle == 0 \|\| (pHandle->iFlags & MEMOBJ_RES) == 0` |
|      42 | 1248 | `	 \|\| VmDirClosed(pHandle) ){` |
|      10 | 1249 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1250 | `			"Directory::%s(): cannot use Directory resource after it has been closed",` |
|       3 | 1251 | `			zMethod);` |
|       - | 1252 | `	}` |
|      36 | 1253 | `	SyStringInitFromBuf(&sStr,zFunc,SyStrlen(zFunc));` |
|      36 | 1254 | `	PH7_MemObjInit(pCtx->pVm,&sName);` |
|      36 | 1255 | `	PH7_MemObjInitFromString(pCtx->pVm,&sName,&sStr);` |
|      36 | 1256 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      36 | 1257 | `	apArg[0] = pHandle;` |
|      36 | 1258 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,&sName,1,apArg,&sResult);` |
|      36 | 1259 | `	PH7_MemObjRelease(&sName);` |
|      36 | 1260 | `	if( rc == SXRET_OK ){` |
|      36 | 1261 | `		ph7_result_value(pCtx,&sResult);` |
|      17 | 1262 | `	}` |
|      36 | 1263 | `	PH7_MemObjRelease(&sResult);` |
|      36 | 1264 | `	return PH7_OK;` |
|      22 | 1265 | `}` |
|      28 | 1266 | `static int vm_builtin_Directory_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1267 | `{` |
|      14 | 1268 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      30 | 1269 | `	return VmDirForward(pCtx,"readdir","read");` |
|       2 | 1270 | `}` |
|       4 | 1271 | `static int vm_builtin_Directory_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1272 | `{` |
|       2 | 1273 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       5 | 1274 | `	return VmDirForward(pCtx,"rewinddir","rewind");` |
|       1 | 1275 | `}` |
|       8 | 1276 | `static int vm_builtin_Directory_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1277 | `{` |
|       4 | 1278 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      10 | 1279 | `	return VmDirForward(pCtx,"closedir","close");` |
|       2 | 1280 | `}` |
|    7925 | 1281 | `static sxi32 VmInstallDirectory(ph7_vm *pVm)` |
|       5 | 1282 | `{` |
|       - | 1283 | `	static const PH7_NativePropDef aDirProp[] = {` |
|       - | 1284 | `		{ DIR_PATH,   PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1285 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|       - | 1286 | `		{ DIR_HANDLE, PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1287 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "mixed" },` |
|       - | 1288 | `	};` |
|       - | 1289 | `	static const PH7_NativeMethodDef aDirMethod[] = {` |
|       - | 1290 | `		{ "close",  PH7_MOD_PUBLIC, "", "void", vm_builtin_Directory_close },` |
|       - | 1291 | `		{ "rewind", PH7_MOD_PUBLIC, "", "void", vm_builtin_Directory_rewind },` |
|       - | 1292 | `		{ "read",   PH7_MOD_PUBLIC, "", "string\|false", vm_builtin_Directory_read },` |
|       - | 1293 | `	};` |
|       - | 1294 | `	static const PH7_NativeClassSpec sSpec = {` |
|       - | 1295 | `		"Directory", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE,` |
|       - | 1296 | `		aDirMethod, SX_ARRAYSIZE(aDirMethod), 0, 0,` |
|       - | 1297 | `		aDirProp, SX_ARRAYSIZE(aDirProp), 0, 0, 0` |
|       - | 1298 | `	};` |
|    7930 | 1299 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|    7930 | 1300 | `	if( rc == SXRET_OK ){` |
|    7930 | 1301 | `		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"Directory",sizeof("Directory")-1,FALSE,0);` |
|    7930 | 1302 | `		if( pClass ){` |
|       - | 1303 | `			/* php words this refusal per class rather than with the generic` |
|       - | 1304 | `			 * "Instantiation of class %s is not allowed". */` |
|    7930 | 1305 | `			pClass->zNewRefusal = "Cannot directly construct Directory, use dir() instead";` |
|    3957 | 1306 | `		}` |
|    3957 | 1307 | `	}` |
|    7930 | 1308 | `	return rc;` |
|       5 | 1309 | `}` |
|       - | 1310 | `/*` |
|       - | 1311 | ` * ---------------------------------------------------------------------------` |
|       - | 1312 | ` * php's attribute classes.` |
|       - | 1313 | ` *` |
|       - | 1314 | ``  * Each carries an ATTRIBUTE of its own — `#[Attribute(Attribute::TARGET_CLASS)]` `` |
|       - | 1315 | ` * on Attribute, a target mask on every other one — and those records are` |
|       - | 1316 | ` * load-bearing rather than decorative: the engine reads them to decide whether a` |
|       - | 1317 | `` * user's `#[Deprecated]` may sit where it does, and ReflectionAttribute answers`` |
|       - | 1318 | ` * them. A compiled attribute holds its argument as byte-code, so this is what` |
|       - | 1319 | `` * `PH7_NativeClassAddAttribute()` exists for (rule 11's next unused corner,`` |
|       - | 1320 | ` * exercised here): the argument rides as a literal.` |
|       - | 1321 | ` *` |
|       - | 1322 | ` * php's Deprecated mask is 87 — TARGET_CLASS\|FUNCTION\|METHOD\|CLASS_CONSTANT\|` |
|       - | 1323 | ` * CONSTANT — where the chunk wrote 86 and left the CLASS bit out.` |
|       - | 1324 | ` *` |
|       - | 1325 | ` * Three of them declare NOTHING but their own mask, because what they mean is a` |
|       - | 1326 | `` * question something else asks: `#[AllowDynamicProperties]` is read by the`` |
|       - | 1327 | `` * dynamic-property decision at the write site, `#[SensitiveParameter]` by the`` |
|       - | 1328 | `` * backtrace builder, `#[ReturnTypeWillChange]` by php's tentative-return-type`` |
|       - | 1329 | ` * check (which the scope policy non-deprecated policy removed, so nothing consults it` |
|       - | 1330 | ` * here). They still have to EXIST: a program that spells one and then asks` |
|       - | 1331 | `` * `getAttributes()[0]->newInstance()` gets php's object, not`` |
|       - | 1332 | `` * `Attribute class "AllowDynamicProperties" not found`.`` |
|       - | 1333 | ` *` |
|       - | 1334 | `` * `SensitiveParameterValue` is not an attribute at all — it is the box php puts`` |
|       - | 1335 | ` * a redacted argument in — but it belongs to the same feature and to the same` |
|       - | 1336 | ` * declaration site.` |
|       - | 1337 | ` * ---------------------------------------------------------------------------` |
|       - | 1338 | ` */` |
|       - | 1339 | ``/* php declares `public function __construct()` on the three marker attributes, so`` |
|       - | 1340 | `` * Reflection reports one and `new AllowDynamicProperties(1)` is an`` |
|       - | 1341 | ` * ArgumentCountError. The body has nothing to do: the object carries no state. */` |
|      10 | 1342 | `static int vm_builtin_AttrMarker_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1343 | `{` |
|       5 | 1344 | `	SXUNUSED(pCtx);` |
|       5 | 1345 | `	SXUNUSED(nArg);` |
|       5 | 1346 | `	SXUNUSED(apArg);` |
|      12 | 1347 | `	return PH7_OK;` |
|       2 | 1348 | `}` |
|       - | 1349 | `/* SensitiveParameterValue::__construct(mixed $value) / getValue() / __debugInfo() */` |
|       - | 1350 | `#define SPV_SLOT "value"` |
|      20 | 1351 | `static int vm_builtin_SensitiveParameterValue_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1352 | `{` |
|      21 | 1353 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      21 | 1354 | `	if( pThis && nArg > 0 ){` |
|      21 | 1355 | `		PH7_NativeSetProp(pCtx->pVm,pThis,SPV_SLOT,sizeof(SPV_SLOT)-1,apArg[0]);` |
|      10 | 1356 | `	}` |
|      21 | 1357 | `	return PH7_OK;` |
|       1 | 1358 | `}` |
|       6 | 1359 | `static int vm_builtin_SensitiveParameterValue_getValue(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1360 | `{` |
|       7 | 1361 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       7 | 1362 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,SPV_SLOT) : 0;` |
|       3 | 1363 | `	SXUNUSED(nArg);` |
|       3 | 1364 | `	SXUNUSED(apArg);` |
|       7 | 1365 | `	if( pVal ){` |
|       7 | 1366 | `		ph7_result_value(pCtx,pVal);` |
|       4 | 1367 | `	}else{` |
|     ! 0 | 1368 | `		ph7_result_null(pCtx);` |
|       - | 1369 | `	}` |
|       7 | 1370 | `	return PH7_OK;` |
|       1 | 1371 | `}` |
|       2 | 1372 | `static int vm_builtin_SensitiveParameterValue_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1373 | `{` |
|       3 | 1374 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|       1 | 1375 | `	SXUNUSED(nArg);` |
|       1 | 1376 | `	SXUNUSED(apArg);` |
|       3 | 1377 | `	if( pOut == 0 ){` |
|     ! 0 | 1378 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1379 | `	}` |
|       3 | 1380 | `	ph7_result_value(pCtx,pOut);` |
|       3 | 1381 | `	return PH7_OK;` |
|       2 | 1382 | `}` |
|       - | 1383 | `/*` |
|       - | 1384 | `` * php gives the class a `get_properties_for` handler that answers NULL for every`` |
|       - | 1385 | `` * purpose, so the box shows nothing to var_export, the `(array)` cast or`` |
|       - | 1386 | `` * json_encode either — not just to var_dump's `__debugInfo()`. The point of the`` |
|       - | 1387 | ` * class is that the value it holds does not leak onto a display surface.` |
|       - | 1388 | ` */` |
|       6 | 1389 | `static sxi32 VmPresentSensitiveParameterValue(ph7_vm *pVm,ph7_class_instance *pThis,` |
|       - | 1390 | `	ph7_value *pOut,int bDebug)` |
|       1 | 1391 | `{` |
|       3 | 1392 | `	SXUNUSED(pVm);` |
|       3 | 1393 | `	SXUNUSED(pThis);` |
|       3 | 1394 | `	SXUNUSED(pOut);` |
|       3 | 1395 | `	SXUNUSED(bDebug);` |
|       7 | 1396 | `	return SXRET_OK;   /* the empty shape, both handlers */` |
|       1 | 1397 | `}` |
|       8 | 1398 | `static int vm_builtin_Attribute_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1399 | `{` |
|       9 | 1400 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       9 | 1401 | `	if( pThis ){` |
|      16 | 1402 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,"flags",` |
|       7 | 1403 | `			nArg > 0 ? ph7_value_to_int64(apArg[0]) : 127);` |
|       4 | 1404 | `	}` |
|       9 | 1405 | `	return PH7_OK;` |
|       1 | 1406 | `}` |
|       - | 1407 | `/* NoDiscard::__construct(?string $message = null) */` |
|       2 | 1408 | `static int vm_builtin_NoDiscard_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1409 | `{` |
|       3 | 1410 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - | 1411 | `	ph7_value sVal;` |
|       3 | 1412 | `	if( pThis == 0 ){` |
|     ! 0 | 1413 | `		return PH7_OK;` |
|       - | 1414 | `	}` |
|       3 | 1415 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|       3 | 1416 | `	if( nArg > 0 ){` |
|     ! 0 | 1417 | `		PH7_MemObjStore(apArg[0],&sVal);` |
|     ! 0 | 1418 | `	}` |
|       3 | 1419 | `	PH7_NativeSetProp(pCtx->pVm,pThis,"message",sizeof("message")-1,&sVal);` |
|       3 | 1420 | `	PH7_MemObjRelease(&sVal);` |
|       3 | 1421 | `	return PH7_OK;` |
|       2 | 1422 | `}` |
|      10 | 1423 | `static int vm_builtin_Deprecated_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1424 | `{` |
|      11 | 1425 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - | 1426 | `	static const char *const azSlot[] = { "message", "since" };` |
|       - | 1427 | `	int n;` |
|      11 | 1428 | `	if( pThis == 0 ){` |
|     ! 0 | 1429 | `		return PH7_OK;` |
|       - | 1430 | `	}` |
|      31 | 1431 | `	for( n = 0 ; n < 2 ; n++ ){` |
|       - | 1432 | `		ph7_value sVal;` |
|      21 | 1433 | `		PH7_MemObjInit(pCtx->pVm,&sVal);` |
|      21 | 1434 | `		if( n < nArg ){` |
|      15 | 1435 | `			PH7_MemObjStore(apArg[n],&sVal);` |
|       7 | 1436 | `		}` |
|      21 | 1437 | `		PH7_NativeSetProp(pCtx->pVm,pThis,azSlot[n],SyStrlen(azSlot[n]),&sVal);` |
|      21 | 1438 | `		PH7_MemObjRelease(&sVal);` |
|      11 | 1439 | `	}` |
|      11 | 1440 | `	return PH7_OK;` |
|       6 | 1441 | `}` |
|    7925 | 1442 | `static sxi32 VmInstallAttributes(ph7_vm *pVm)` |
|       5 | 1443 | `{` |
|       - | 1444 | `	static const PH7_NativeConstDef aAttrConst[] = {` |
|       - | 1445 | `		{ "TARGET_CLASS",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },` |
|       - | 1446 | `		{ "TARGET_FUNCTION",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|       - | 1447 | `		{ "TARGET_METHOD",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4, 0, 0.0 },` |
|       - | 1448 | `		{ "TARGET_PROPERTY",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 8, 0, 0.0 },` |
|       - | 1449 | `		{ "TARGET_CLASS_CONSTANT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16, 0, 0.0 },` |
|       - | 1450 | `		{ "TARGET_PARAMETER",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },` |
|       - | 1451 | `		{ "TARGET_CONSTANT",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64, 0, 0.0 },` |
|       - | 1452 | `		{ "TARGET_ALL",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 127, 0, 0.0 },` |
|       - | 1453 | `		{ "IS_REPEATABLE",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 128, 0, 0.0 },` |
|       - | 1454 | `	};` |
|       - | 1455 | ``	/* php declares `public int $flags;` — typed, NO default (the constructor is`` |
|       - | 1456 | ``	 * the only writer), which is what the chunk's `public $flags;` could not say. */`` |
|       - | 1457 | `	static const PH7_NativePropDef aAttrProp[] = {` |
|       - | 1458 | `		{ "flags", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|       - | 1459 | `	};` |
|       - | 1460 | `	static const PH7_NativeMethodDef aAttrMethod[] = {` |
|       - | 1461 | `		{ "__construct", PH7_MOD_PUBLIC, "int $flags = Attribute::TARGET_ALL", 0,` |
|       - | 1462 | `		  vm_builtin_Attribute_construct },` |
|       - | 1463 | `	};` |
|       - | 1464 | `	static const PH7_NativePropDef aDepProp[] = {` |
|       - | 1465 | `		{ "message", PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1466 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "?string" },` |
|       - | 1467 | `		{ "since",   PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1468 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "?string" },` |
|       - | 1469 | `	};` |
|       - | 1470 | `	static const PH7_NativeMethodDef aDepMethod[] = {` |
|       - | 1471 | `		{ "__construct", PH7_MOD_PUBLIC, "?string $message = null, ?string $since = null", 0,` |
|       - | 1472 | `		  vm_builtin_Deprecated_construct },` |
|       - | 1473 | `	};` |
|       - | 1474 | ``	/* NoDiscard is Deprecated's shape minus the `since`. */`` |
|       - | 1475 | `	static const PH7_NativePropDef aNdProp[] = {` |
|       - | 1476 | `		{ "message", PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1477 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "?string" },` |
|       - | 1478 | `	};` |
|       - | 1479 | `	static const PH7_NativeMethodDef aNdMethod[] = {` |
|       - | 1480 | `		{ "__construct", PH7_MOD_PUBLIC, "?string $message = null", 0,` |
|       - | 1481 | `		  vm_builtin_NoDiscard_construct },` |
|       - | 1482 | `	};` |
|       - | 1483 | `	/* The three markers: one argless constructor each and no state at all. */` |
|       - | 1484 | `	static const PH7_NativeMethodDef aMarkerMethod[] = {` |
|       - | 1485 | `		{ "__construct", PH7_MOD_PUBLIC, "", 0, vm_builtin_AttrMarker_construct },` |
|       - | 1486 | `	};` |
|       - | 1487 | `	static const PH7_NativePropDef aSpvProp[] = {` |
|       - | 1488 | `		{ SPV_SLOT, PH7_MOD_PRIVATE\|PH7_MOD_READONLY,` |
|       - | 1489 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "mixed" },` |
|       - | 1490 | `	};` |
|       - | 1491 | `	static const PH7_NativeMethodDef aSpvMethod[] = {` |
|       - | 1492 | `		{ "__construct", PH7_MOD_PUBLIC, "mixed $value", 0,` |
|       - | 1493 | `		  vm_builtin_SensitiveParameterValue_construct },` |
|       - | 1494 | `		{ "getValue",    PH7_MOD_PUBLIC, "", "mixed",` |
|       - | 1495 | `		  vm_builtin_SensitiveParameterValue_getValue },` |
|       - | 1496 | `		{ "__debugInfo", PH7_MOD_PUBLIC, "", "array",` |
|       - | 1497 | `		  vm_builtin_SensitiveParameterValue_debugInfo },` |
|       - | 1498 | `	};` |
|       - | 1499 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1500 | `		{ "Attribute", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1501 | `		  aAttrMethod, SX_ARRAYSIZE(aAttrMethod), aAttrConst, SX_ARRAYSIZE(aAttrConst),` |
|       - | 1502 | `		  aAttrProp, SX_ARRAYSIZE(aAttrProp), 0, 0, 0 },` |
|       - | 1503 | `		{ "Deprecated", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1504 | `		  aDepMethod, SX_ARRAYSIZE(aDepMethod), 0, 0,` |
|       - | 1505 | `		  aDepProp, SX_ARRAYSIZE(aDepProp), 0, 0, 0 },` |
|       - | 1506 | `		{ "AllowDynamicProperties", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1507 | `		  aMarkerMethod, SX_ARRAYSIZE(aMarkerMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1508 | `		{ "SensitiveParameter", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1509 | `		  aMarkerMethod, SX_ARRAYSIZE(aMarkerMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1510 | `		{ "ReturnTypeWillChange", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1511 | `		  aMarkerMethod, SX_ARRAYSIZE(aMarkerMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1512 | `		{ "Override", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1513 | `		  aMarkerMethod, SX_ARRAYSIZE(aMarkerMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1514 | `		{ "NoDiscard", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1515 | `		  aNdMethod, SX_ARRAYSIZE(aNdMethod), 0, 0,` |
|       - | 1516 | `		  aNdProp, SX_ARRAYSIZE(aNdProp), 0, 0, 0 },` |
|       - | 1517 | `		/* php 8.5's marker for an attribute whose TARGET is checked late. It is` |
|       - | 1518 | `		 * the one attribute class php declares with no constructor at all --` |
|       - | 1519 | `		 * every other marker here has the empty one -- so a script writes it` |
|       - | 1520 | ``		 * bare and `newInstance()` builds it with nothing. */`` |
|       - | 1521 | `		{ "DelayedTargetValidation", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1522 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1523 | `		/* php refuses BOTH directions for the box (ZEND_ACC_NOT_SERIALIZABLE), which` |
|       - | 1524 | `		 * is the whole point: a redacted value must not reach a payload either. */` |
|       - | 1525 | `		{ "SensitiveParameterValue", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOSERIALIZE,` |
|       - | 1526 | `		  aSpvMethod, SX_ARRAYSIZE(aSpvMethod), 0, 0,` |
|       - | 1527 | `		  aSpvProp, SX_ARRAYSIZE(aSpvProp), 0, 0,` |
|       - | 1528 | `		  VmPresentSensitiveParameterValue },` |
|       - | 1529 | `	};` |
|       - | 1530 | ``	/* Each attribute class's own `#[Attribute(mask)]`, php's masks verbatim. The`` |
|       - | 1531 | `	 * literal rows are STATIC because PH7_NativeClassAddAttribute keeps a pointer` |
|       - | 1532 | `	 * to them for the VM's lifetime. */` |
|       - | 1533 | `	static const PH7_NativeAttrArg aMaskClass[]  = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 1,  0, 0.0 } } };` |
|       - | 1534 | `	static const PH7_NativeAttrArg aMaskDep[]    = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 87, 0, 0.0 } } };` |
|       - | 1535 | `	static const PH7_NativeAttrArg aMaskParam[]  = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 32, 0, 0.0 } } };` |
|       - | 1536 | `	static const PH7_NativeAttrArg aMaskMethod[] = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 4,  0, 0.0 } } };` |
|       - | 1537 | `	static const PH7_NativeAttrArg aMaskMembr[]  = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 12, 0, 0.0 } } };` |
|       - | 1538 | `	static const PH7_NativeAttrArg aMaskCallee[] = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 6,  0, 0.0 } } };` |
|       - | 1539 | `	static const PH7_NativeAttrArg aMaskAll[]    = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 127,0, 0.0 } } };` |
|       - | 1540 | `	static const struct {` |
|       - | 1541 | `		const char *zClass;` |
|       - | 1542 | `		const PH7_NativeAttrArg *aArg;   /* php's TARGET_* mask for that class */` |
|       - | 1543 | `	} aOwnAttr[] = {` |
|       - | 1544 | `		{ "Attribute",              aMaskClass  },   /* TARGET_CLASS */` |
|       - | 1545 | `		{ "Deprecated",             aMaskDep    },   /* CLASS\|FUNCTION\|METHOD\|CLASS_CONSTANT\|CONSTANT */` |
|       - | 1546 | `		{ "AllowDynamicProperties", aMaskClass  },   /* TARGET_CLASS */` |
|       - | 1547 | `		{ "SensitiveParameter",     aMaskParam  },   /* TARGET_PARAMETER */` |
|       - | 1548 | `		{ "ReturnTypeWillChange",   aMaskMethod },   /* TARGET_METHOD */` |
|       - | 1549 | `		{ "Override",               aMaskMembr  },   /* METHOD\|PROPERTY (php 8.5) */` |
|       - | 1550 | `		{ "NoDiscard",              aMaskCallee },   /* FUNCTION\|METHOD (php 8.5) */` |
|       - | 1551 | `		{ "DelayedTargetValidation", aMaskAll   },   /* TARGET_ALL (php 8.5) */` |
|       - | 1552 | `	};` |
|    7930 | 1553 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       - | 1554 | `	sxu32 n;` |
|   71330 | 1555 | `	for( n = 0 ; rc == SXRET_OK && n < SX_ARRAYSIZE(aOwnAttr) ; ++n ){` |
|   95061 | 1556 | `		rc = PH7_NativeClassAddAttribute(&(*pVm),` |
|   95056 | 1557 | `			PH7_VmExtractClass(&(*pVm),aOwnAttr[n].zClass,` |
|   63400 | 1558 | `				(sxu32)SyStrlen(aOwnAttr[n].zClass),FALSE,0),` |
|   63400 | 1559 | `			"Attribute",aOwnAttr[n].aArg,1);` |
|   31661 | 1560 | `	}` |
|    7930 | 1561 | `	return rc;` |
|       5 | 1562 | `}` |
|       - | 1563 | `/*` |
|       - | 1564 | ` * stdClass and Random\RandomException.` |
|       - | 1565 | ` *` |
|       - | 1566 | ` * stdClass is EMPTY in php too — it holds only dynamic properties — so the whole` |
|       - | 1567 | `` * declaration is the row. `Random\RandomException` is the first NAMESPACED class`` |
|       - | 1568 | ` * declared from C: the engine keys its class table by the FULLY QUALIFIED name` |
|       - | 1569 | `` * (the compiler resolves `namespace Random { class RandomException }` to exactly`` |
|       - | 1570 | ` * this string before installing), so a spec row spells the FQN and needs no` |
|       - | 1571 | ` * namespace machinery at all. It also retires the chunk this file kept ALONE for` |
|       - | 1572 | `` * it, whose comment explains why: a `namespace` declaration is not reset at its`` |
|       - | 1573 | ` * closing brace here, so anything following it in the same chunk would have` |
|       - | 1574 | ` * leaked into the Random namespace.` |
|       - | 1575 | ` */` |
|    7925 | 1576 | `static sxi32 VmInstallStdClasses(ph7_vm *pVm)` |
|       5 | 1577 | `{` |
|       - | 1578 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1579 | `		{ "stdClass", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1580 | `		/* unserialize()'s carrier for a disallowed or unknown class: as empty as` |
|       - | 1581 | `		 * stdClass (its properties are the payload's, created dynamically); what` |
|       - | 1582 | `		 * makes it special is the pVm->pIncClass checks at the access sites. */` |
|       - | 1583 | `		{ "__PHP_Incomplete_Class", 0, 0, PH7_CLASS_FINAL, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1584 | `		{ "Random\\RandomException", "Exception", 0, PH7_CLASS_NOCLONE,` |
|       - | 1585 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1586 | `		/* php 8.5's filter exceptions: FILTER_THROW_ON_FAILURE raises the second,` |
|       - | 1587 | `		 * and the first is the base a caller catches to mean "any filter error". */` |
|       - | 1588 | `		{ "Filter\\FilterException", "Exception", 0, 0,` |
|       - | 1589 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1590 | `		{ "Filter\\FilterFailedException", "Filter\\FilterException", 0, 0,` |
|       - | 1591 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1592 | `	};` |
|    7930 | 1593 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       5 | 1594 | `}` |
|    7925 | 1595 | `PH7_PRIVATE sxi32 PH7_VmInstallBuiltinLib(ph7_vm *pVm)` |
|       5 | 1596 | `{` |
|       - | 1597 | `	SyString sBuiltin;` |
|       - | 1598 | `	/* The interfaces first: everything below implements one of them` |
|       - | 1599 | `	 * (Exception implements Throwable). */` |
|    7930 | 1600 | `	VmInstallCoreInterfaces(&(*pVm));` |
|    7930 | 1601 | `	VmInstallExceptions(&(*pVm));` |
|    7930 | 1602 | `	VmInstallStdClasses(&(*pVm));` |
|    7930 | 1603 | `	VmInstallDirectory(&(*pVm));` |
|    7930 | 1604 | `	VmInstallAttributes(&(*pVm));` |
|    7930 | 1605 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|       - | 1606 | `	/* Compile the built-in library */` |
|    7930 | 1607 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|    7930 | 1608 | `	return SXRET_OK;` |
|       5 | 1609 | `}` |
|       - | 1610 |  |
