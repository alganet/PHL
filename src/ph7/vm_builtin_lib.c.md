# src/ph7/vm_builtin_lib.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 500/568 lines (88.03%)

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
|       - |  305 | `   "function array_count_values(array $array): array {"\` |
|       - |  306 | `   "  $out = array();"\` |
|       - |  307 | `   "  foreach( $array as $v ){"\` |
|       - |  308 | `   "    if( !is_int($v) && !is_string($v) ){"\` |
|       - |  309 | `   "      trigger_error('array_count_values(): Can only count string and integer values, entry skipped', E_USER_WARNING);"\` |
|       - |  310 | `   "      continue;"\` |
|       - |  311 | `   "    }"\` |
|       - |  312 | `   "    if( isset($out[$v]) ){ $out[$v] = $out[$v] + 1; } else { $out[$v] = 1; }"\` |
|       - |  313 | `   "  }"\` |
|       - |  314 | `   "  return $out;"\` |
|       - |  315 | `   "}"\` |
|       - |  316 | `   "function array_change_key_case(array $array, int $case = CASE_LOWER): array {"\` |
|       - |  317 | `   "  $out = array();"\` |
|       - |  318 | `   "  foreach( $array as $k => $v ){"\` |
|       - |  319 | `   "    if( is_string($k) ){ $k = ($case == CASE_UPPER) ? strtoupper($k) : strtolower($k); }"\` |
|       - |  320 | `   "    $out[$k] = $v;"\` |
|       - |  321 | `   "  }"\` |
|       - |  322 | `   "  return $out;"\` |
|       - |  323 | `   "}"\` |
|       - |  324 | `   "function array_replace_recursive(array $array, array ...$replacements): array {"\` |
|       - |  325 | `   "  foreach( $replacements as $o ){"\` |
|       - |  326 | `   "    foreach( $o as $k => $v ){"\` |
|       - |  327 | `   "      if( is_array($v) && isset($array[$k]) && is_array($array[$k]) ){"\` |
|       - |  328 | `   "        $array[$k] = array_replace_recursive($array[$k], $v);"\` |
|       - |  329 | `   "      }else{"\` |
|       - |  330 | `   "        $array[$k] = $v;"\` |
|       - |  331 | `   "      }"\` |
|       - |  332 | `   "    }"\` |
|       - |  333 | `   "  }"\` |
|       - |  334 | `   "  return $array;"\` |
|       - |  335 | `   "}"\` |
|       - |  336 | `   /* class_parents/class_implements/class_uses moved to C (vm_builtin_class.c):` |
|       - |  337 | `    * as prelude wrappers they gated on class_exists(), so an interface, a trait` |
|       - |  338 | `    * and an enum all answered FALSE where php answers a list; class_uses could` |
|       - |  339 | `    * not reach the trait table at all and returned the empty set for every class;` |
|       - |  340 | `    * and the E_WARNING php raises for a name nothing declares cannot be raised` |
|       - |  341 | `    * from here at php's severity or against the CALLER's line. */\` |
|       - |  342 | `   "function ip2long(string $ip): int\|false {"\` |
|       - |  343 | `   "  $p = explode('.', $ip);"\` |
|       - |  344 | `   "  if( count($p) !== 4 ){ return false; }"\` |
|       - |  345 | `   "  $n = 0;"\` |
|       - |  346 | `   "  foreach( $p as $o ){"\` |
|       - |  347 | `   "    if( !ctype_digit($o) \|\| (int)$o < 0 \|\| (int)$o > 255 ){ return false; }"\` |
|       - |  348 | `   "    $n = $n * 256 + (int)$o;"\` |
|       - |  349 | `   "  }"\` |
|       - |  350 | `   "  return $n;"\` |
|       - |  351 | `   "}"\` |
|       - |  352 | `   "function long2ip(int $ip): string {"\` |
|       - |  353 | `   "  return (($ip >> 24) & 255) . '.' . (($ip >> 16) & 255) . '.' . (($ip >> 8) & 255) . '.' . ($ip & 255);"\` |
|       - |  354 | `   "}"\` |
|       - |  355 | `   "/* php 8.3 str_increment(): Perl-style alphanumeric increment. */"\` |
|       - |  356 | `   "function str_increment(string $string): string {"\` |
|       - |  357 | `   "  if( $string === '' ){ throw new ValueError('str_increment(): Argument #1 ($string) must not be empty'); }"\` |
|       - |  358 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_increment(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|       - |  359 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|       - |  360 | `   "    $c = $string[$i];"\` |
|       - |  361 | `   "    if( $c === 'z' ){ $string[$i] = 'a'; }"\` |
|       - |  362 | `   "    elseif( $c === 'Z' ){ $string[$i] = 'A'; }"\` |
|       - |  363 | `   "    elseif( $c === '9' ){ $string[$i] = '0'; }"\` |
|       - |  364 | `   "    else { $string[$i] = chr(ord($c) + 1); return $string; }"\` |
|       - |  365 | `   "  }"\` |
|       - |  366 | `   "  $first = $string[0];"\` |
|       - |  367 | `   "  if( $first === '0' ){ return '1' . $string; }"\` |
|       - |  368 | `   "  if( $first === 'a' ){ return 'a' . $string; }"\` |
|       - |  369 | `   "  return 'A' . $string;"\` |
|       - |  370 | `   "}"\` |
|       - |  371 | `   "/* php 8.3 str_decrement(): inverse of str_increment(); throws out of range"\` |
|       - |  372 | `   " * at the bottom of the counting sequence. */"\` |
|       - |  373 | `   "function str_decrement(string $string): string {"\` |
|       - |  374 | `   "  if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) must not be empty'); }"\` |
|       - |  375 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_decrement(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|       - |  376 | `   "  $orig = $string;"\` |
|       - |  377 | `   "  $borrowed = false;"\` |
|       - |  378 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|       - |  379 | `   "    $c = $string[$i];"\` |
|       - |  380 | `   "    if( $c === 'a' ){ $string[$i] = 'z'; }"\` |
|       - |  381 | `   "    elseif( $c === 'A' ){ $string[$i] = 'Z'; }"\` |
|       - |  382 | `   "    elseif( $c === '0' ){ $string[$i] = '9'; }"\` |
|       - |  383 | `   "    else { $string[$i] = chr(ord($c) - 1); $borrowed = false; break; }"\` |
|       - |  384 | `   "    if( $i === 0 ){ $borrowed = true; }"\` |
|       - |  385 | `   "  }"\` |
|       - |  386 | `   "  if( $borrowed ){"\` |
|       - |  387 | `   "    if( $string[0] === '9' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|       - |  388 | `   "    $string = substr($string, 1);"\` |
|       - |  389 | `   "    if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|       - |  390 | `   "  } elseif( strlen($string) > 1 && $string[0] === '0' ){"\` |
|       - |  391 | `   "    $string = substr($string, 1);"\` |
|       - |  392 | `   "  }"\` |
|       - |  393 | `   "  return $string;"\` |
|       - |  394 | `   "}"\` |
|       - |  395 | `   /* fileperms/fileowner/filegroup/fileinode moved to C (vfs.c, VfsStatField):` |
|       - |  396 | `    * as prelude wrappers over stat() three of them said nothing on a failed stat` |
|       - |  397 | `    * and the fourth raised trigger_error, whose errno is E_USER_WARNING's 512 and` |
|       - |  398 | `    * whose line is this chunk's rather than the caller's. */\` |
|       - |  399 | `   "/* PH7 keeps no stat cache, so this is a no-op like php on a clean cache. */"\` |
|       - |  400 | `   "function clearstatcache(bool $clear_realpath_cache = false, string $filename = ''): void {}"\` |
|       - |  401 | `   /* mb_ucfirst/mb_lcfirst moved to C (builtin_mb.c): as prelude wrappers they` |
|       - |  402 | `    * dropped $encoding, UPPER-cased where php title-cases ('ß' -> 'SS' for php's` |
|       - |  403 | `    * 'Ss') and lowered a leading Σ with nothing after it, which is php's FINAL` |
|       - |  404 | `    * sigma and not what a first character gets. */\` |
|       - |  405 | `   "/* Creates a temporary file and returns its name */"\` |
|       - |  406 | `   "function tempnam(string $directory,string $prefix): string\|false"\` |
|       - |  407 | `   "{"\` |
|       - |  408 | `   "   /* php's Z_PARAM_PATH refusal on BOTH parameters (see scandir above); the prefix"\` |
|       - |  409 | `   "    * is a path fragment there too, and PHL used to build a filename with the NUL"\` |
|       - |  410 | `   "    * still in it. */"\` |
|       - |  411 | `   "   if( strpos($directory, chr(0)) !== false ){"\` |
|       - |  412 | `   "     throw new ValueError('tempnam(): Argument #1 ($directory) must not contain any null bytes');"\` |
|       - |  413 | `   "   }"\` |
|       - |  414 | `   "   if( strpos($prefix, chr(0)) !== false ){"\` |
|       - |  415 | `   "     throw new ValueError('tempnam(): Argument #2 ($prefix) must not contain any null bytes');"\` |
|       - |  416 | `   "   }"\` |
|       - |  417 | `   "   /* php falls back to the system temporary directory when the one it was"\` |
|       - |  418 | `   "    * given cannot HOLD the file, and says so -- except for the empty"\` |
|       - |  419 | ``   "    * directory, which it reads as `use the temp dir` and answers silently."\`` |
|       - |  420 | `   "    * PHL took '' literally and spent 64 tries failing at the filesystem"\` |
|       - |  421 | `   "    * ROOT. Whether a directory can hold it is settled by TRYING, not by"\` |
|       - |  422 | `   "    * asking is_writable(): the two disagree on Windows. */"\` |
|       - |  423 | `   "   $zTmp = rtrim(sys_get_temp_dir(), DIRECTORY_SEPARATOR);"\` |
|       - |  424 | `   "   $zDir = $directory === '' ? $zTmp : rtrim($directory, DIRECTORY_SEPARATOR);"\` |
|       - |  425 | `   "   if( is_dir($zDir) && is_writable($zDir) ){"\` |
|       - |  426 | `   "     $zOut = __tempnam_in($zDir, $prefix);"\` |
|       - |  427 | `   "     if( $zOut !== false ){ return $zOut; }"\` |
|       - |  428 | `   "   }"\` |
|       - |  429 | `   "   if( $zDir === $zTmp ){ return false; }"\` |
|       - |  430 | `   "   trigger_error(\"tempnam(): file created in the system's temporary directory\", E_USER_NOTICE);"\` |
|       - |  431 | `   "   return __tempnam_in($zTmp, $prefix);"\` |
|       - |  432 | `   "}"\` |
|       - |  433 | `   "function __tempnam_in(string $zDir, string $prefix)"\` |
|       - |  434 | `   "{"\` |
|       - |  435 | `   "   /* php CREATES the file (empty, mode 0600) and guarantees the name is"\` |
|       - |  436 | `   "    * unique -- returning a bare name left the caller with a path that does"\` |
|       - |  437 | `   "    * not exist, so file_exists() was false and unlink() failed on it. */"\` |
|       - |  438 | `   "   for( $i = 0 ; $i < 64 ; ++$i ){"\` |
|       - |  439 | `   "     $zPath = $zDir.DIRECTORY_SEPARATOR.$prefix.rand_str(12);"\` |
|       - |  440 | `   "     if( file_exists($zPath) ){ continue; }"\` |
|       - |  441 | `   "     $pHandle = @fopen($zPath,'x');"\` |
|       - |  442 | `   "     if( $pHandle === false ){ return false; }"\` |
|       - |  443 | `   "     fclose($pHandle);"\` |
|       - |  444 | `   "     @chmod($zPath, 0600);"\` |
|       - |  445 | `   "     return $zPath;"\` |
|       - |  446 | `   "   }"\` |
|       - |  447 | `   "   return false;"\` |
|       - |  448 | `   "}"\` |
|       - |  449 | `	/* fileowner/filegroup/fileinode: see the note beside fileperms above. */\` |
|       - |  450 | `	""` |
|       - |  451 |  |
|       - |  452 | `/*` |
|       - |  453 | ` * ---------------------------------------------------------------------------` |
|       - |  454 | ` * The Exception / Error family, declared from C.` |
|       - |  455 | ` *` |
|       - |  456 | ` * php's two roots are one implementation twice over (its stub says` |
|       - |  457 | `` * `@implementation-alias Exception::__construct` for every one of Error's`` |
|       - |  458 | ` * methods), so the bodies below are shared by both spec tables and the` |
|       - |  459 | ` * ~20 subclasses are declaration-only rows.` |
|       - |  460 | ` *` |
|       - |  461 | `` * php's seven slots, in php's own declaration order. `string` is php's cache of`` |
|       - |  462 | ` * the __toString rendering -- unused by the engine but PRESENT on every` |
|       - |  463 | ` * presentation surface, which is why it is declared here rather than skipped:` |
|       - |  464 | ` * var_dump/print_r/(array)/serialize all show it, and PHL was one property short` |
|       - |  465 | ` * of php on every exception ever printed.` |
|       - |  466 | ` * ---------------------------------------------------------------------------` |
|       - |  467 | ` */` |
|       - |  468 | `#define EXC_MESSAGE  "message"` |
|       - |  469 | `#define EXC_STRING   "string"` |
|       - |  470 | `#define EXC_CODE     "code"` |
|       - |  471 | `#define EXC_FILE     "file"` |
|       - |  472 | `#define EXC_LINE     "line"` |
|       - |  473 | `#define EXC_TRACE    "trace"` |
|       - |  474 | `#define EXC_PREVIOUS "previous"` |
|       - |  475 | `#define EXC_SEVERITY "severity"` |
|       - |  476 | `/*` |
|       - |  477 | ` * Answer a declared slot the way php's getter does. Three of the seven CONVERT` |
|       - |  478 | ` * rather than copy — getMessage()/getFile() answer a string and getLine() an int,` |
|       - |  479 | ` * whatever the slot holds — and that shows twice: a subclass assigning` |
|       - |  480 | `` * `$this->message = 5` reads back "5", and a slot __wakeup has DROPPED reads as`` |
|       - |  481 | ` * "" rather than null. The other four are verbatim copies (getCode() of that same` |
|       - |  482 | ` * subclass really is the int).` |
|       - |  483 | ` */` |
|       - |  484 | `#define EXC_READ_RAW 0` |
|       - |  485 | `#define EXC_READ_STR 1` |
|       - |  486 | `#define EXC_READ_INT 2` |
|   24021 |  487 | `static int VmExcReadSlot(ph7_context *pCtx,const char *zSlot,int iAs)` |
|       5 |  488 | `{` |
|   24026 |  489 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   24026 |  490 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,zSlot) : 0;` |
|       - |  491 | `	ph7_value sTmp;` |
|   24026 |  492 | `	if( iAs == EXC_READ_RAW ){` |
|    1905 |  493 | `		if( pVal ){` |
|    1905 |  494 | `			ph7_result_value(pCtx,pVal);` |
|     955 |  495 | `		}else{` |
|     ! 0 |  496 | `			ph7_result_null(pCtx);` |
|       - |  497 | `		}` |
|    1905 |  498 | `		return PH7_OK;` |
|       - |  499 | `	}` |
|       - |  500 | `	/* Through a COPY: converting the slot would rewrite the exception's state. */` |
|   22126 |  501 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|   22126 |  502 | `	if( pVal ){` |
|   22126 |  503 | `		PH7_MemObjStore(pVal,&sTmp);` |
|   11033 |  504 | `	}` |
|   22126 |  505 | `	if( iAs == EXC_READ_INT ){` |
|    1481 |  506 | `		PH7_MemObjToInteger(&sTmp);` |
|     743 |  507 | `	}else{` |
|   20650 |  508 | `		PH7_MemObjToString(&sTmp);` |
|       - |  509 | `	}` |
|   22126 |  510 | `	ph7_result_value(pCtx,&sTmp);` |
|   22126 |  511 | `	PH7_MemObjRelease(&sTmp);` |
|   22126 |  512 | `	return PH7_OK;` |
|   11988 |  513 | `}` |
|   19931 |  514 | `static int vm_builtin_Exception_getMessage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  515 | `{` |
|    9938 |  516 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   19936 |  517 | `	return VmExcReadSlot(pCtx,EXC_MESSAGE,EXC_READ_STR);` |
|       5 |  518 | `}` |
|    1398 |  519 | `static int vm_builtin_Exception_getCode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  520 | `{` |
|     699 |  521 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    1403 |  522 | `	return VmExcReadSlot(pCtx,EXC_CODE,EXC_READ_RAW);` |
|       5 |  523 | `}` |
|     714 |  524 | `static int vm_builtin_Exception_getFile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  525 | `{` |
|     357 |  526 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     719 |  527 | `	return VmExcReadSlot(pCtx,EXC_FILE,EXC_READ_STR);` |
|       5 |  528 | `}` |
|    1476 |  529 | `static int vm_builtin_Exception_getLine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  530 | `{` |
|     738 |  531 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    1481 |  532 | `	return VmExcReadSlot(pCtx,EXC_LINE,EXC_READ_INT);` |
|       5 |  533 | `}` |
|     378 |  534 | `static int vm_builtin_Exception_getTrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  535 | `{` |
|     189 |  536 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     383 |  537 | `	return VmExcReadSlot(pCtx,EXC_TRACE,EXC_READ_RAW);` |
|       5 |  538 | `}` |
|     118 |  539 | `static int vm_builtin_Exception_getPrevious(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  540 | `{` |
|      59 |  541 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     120 |  542 | `	return VmExcReadSlot(pCtx,EXC_PREVIOUS,EXC_READ_RAW);` |
|       2 |  543 | `}` |
|       6 |  544 | `static int vm_builtin_ErrorException_getSeverity(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  545 | `{` |
|       3 |  546 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       8 |  547 | `	return VmExcReadSlot(pCtx,EXC_SEVERITY,EXC_READ_RAW);` |
|       2 |  548 | `}` |
|       - |  549 | `/*` |
|       - |  550 | ` * php's zend_update_exception_properties: each of the three is written only when` |
|       - |  551 | ``  * the caller actually supplied it — a message when the argument was PASSED (`""` `` |
|       - |  552 | ` * included), a code when it is NON-ZERO, a previous when it is an object. That is` |
|       - |  553 | ` * not the same as writing the defaults: a subclass may redeclare` |
|       - |  554 | `` * `protected $message = 'default'`, and php keeps it for `new Sub()`.`` |
|       - |  555 | ` */` |
| 1469883 |  556 | `static void VmExcInitProps(ph7_context *pCtx,ph7_class_instance *pThis,int nArg,` |
|       - |  557 | `	ph7_value **apArg,int iPrev)` |
|       5 |  558 | `{` |
| 1469888 |  559 | `	if( nArg > 0 ){` |
| 1469692 |  560 | `		int nMsg = 0;` |
| 1469692 |  561 | `		const char *zMsg = ph7_value_to_string(apArg[0],&nMsg);` |
| 1469692 |  562 | `		PH7_NativeSetAttrStr(pCtx->pVm,pThis,EXC_MESSAGE,zMsg,nMsg);` |
|  734816 |  563 | `	}` |
| 1469888 |  564 | `	if( nArg > 1 ){` |
|       - |  565 | `		ph7_value sCode;` |
|    1401 |  566 | `		PH7_MemObjInit(pCtx->pVm,&sCode);` |
|    1401 |  567 | `		PH7_MemObjStore(apArg[1],&sCode);` |
|    1401 |  568 | `		PH7_MemObjToInteger(&sCode);` |
|    1401 |  569 | `		if( sCode.x.iVal != 0 ){` |
|    1361 |  570 | `			PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_CODE,sCode.x.iVal);` |
|     678 |  571 | `		}` |
|    1401 |  572 | `		PH7_MemObjRelease(&sCode);` |
|     698 |  573 | `	}` |
|       - |  574 | ``	/* php's `previous` is the LAST parameter of each constructor, and`` |
|       - |  575 | `	 * ErrorException's is #5 rather than #2. */` |
| 1469888 |  576 | `	if( nArg > iPrev && (apArg[iPrev]->iFlags & MEMOBJ_OBJ) && apArg[iPrev]->x.pOther ){` |
|      41 |  577 | `		PH7_NativeSetAttrObj(pCtx->pVm,pThis,EXC_PREVIOUS,` |
|      24 |  578 | `			(ph7_class_instance *)apArg[iPrev]->x.pOther);` |
|      12 |  579 | `	}` |
| 1469888 |  580 | `}` |
| 1469837 |  581 | `static int vm_builtin_Exception_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  582 | `{` |
| 1469842 |  583 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
| 1469842 |  584 | `	if( pThis ){` |
| 1469842 |  585 | `		VmExcInitProps(pCtx,pThis,nArg,apArg,2);` |
|  734891 |  586 | `	}` |
| 1469842 |  587 | `	return PH7_OK;` |
|       5 |  588 | `}` |
|       - |  589 | `/*` |
|       - |  590 | ` * ErrorException's own constructor: php's Exception three, then severity, then` |
|       - |  591 | `` * the OPTIONAL file/line overrides. php's `?string $filename = null` /`` |
|       - |  592 | `` * `?int $line = null` mean "keep the creation site" — the chunk defaulted them to`` |
|       - |  593 | ` * __FILE__/__LINE__, which resolved against the EMBEDDED chunk and reported` |
|       - |  594 | `` * `:MEMORY:` line 1 for every ErrorException that did not pass them. php's one`` |
|       - |  595 | ` * asymmetry: a filename WITHOUT a line resets the line to 0.` |
|       - |  596 | ` */` |
|      46 |  597 | `static int vm_builtin_ErrorException_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  598 | `{` |
|      49 |  599 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      49 |  600 | `	if( pThis == 0 ){` |
|     ! 0 |  601 | `		return PH7_OK;` |
|       - |  602 | `	}` |
|      49 |  603 | `	VmExcInitProps(pCtx,pThis,nArg,apArg,5);` |
|      49 |  604 | `	if( nArg > 2 ){` |
|      22 |  605 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_SEVERITY,ph7_value_to_int64(apArg[2]));` |
|      10 |  606 | `	}` |
|      49 |  607 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|      12 |  608 | `		int nFile = 0;` |
|      12 |  609 | `		const char *zFile = ph7_value_to_string(apArg[3],&nFile);` |
|      12 |  610 | `		PH7_NativeSetAttrStr(pCtx->pVm,pThis,EXC_FILE,zFile,nFile);` |
|      12 |  611 | `		if( nArg < 5 \|\| ph7_value_is_null(apArg[4]) ){` |
|       3 |  612 | `			PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_LINE,0);` |
|       1 |  613 | `		}` |
|       5 |  614 | `	}` |
|      49 |  615 | `	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|      10 |  616 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_LINE,ph7_value_to_int64(apArg[4]));` |
|       4 |  617 | `	}` |
|      49 |  618 | `	return PH7_OK;` |
|      26 |  619 | `}` |
|       - |  620 | `/*` |
|       - |  621 | ` * php's private __clone. It has an empty body and is never reached: the class` |
|       - |  622 | ` * carries php's own clone refusal (PH7_CLASS_NOCLONE, answered before any body` |
|       - |  623 | `` * runs), which is what `clone $e` reports — "Trying to clone an uncloneable`` |
|       - |  624 | ` * object of class X", not a visibility error. Declaring it is still php-visible:` |
|       - |  625 | `` * Reflection lists it, and `$e->__clone()` from inside the class works.`` |
|       - |  626 | ` */` |
|     ! 0 |  627 | `static int vm_builtin_Exception_clone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  628 | `{` |
|     ! 0 |  629 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  630 | `	ph7_result_null(pCtx);` |
|     ! 0 |  631 | `	return PH7_OK;` |
|     ! 0 |  632 | `}` |
|       - |  633 | `/*` |
|       - |  634 | ` * php's __wakeup: the two UNTYPED slots are the only ones a serialized payload` |
|       - |  635 | ` * can lie about (the other five are typed and the store enforces them), so php` |
|       - |  636 | ` * DROPS a message that is not a string and a code that is not an int rather than` |
|       - |  637 | ` * letting a method read one.` |
|       - |  638 | ` */` |
|     ! 0 |  639 | `static void VmExcDropSlot(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot)` |
|     ! 0 |  640 | `{` |
|     ! 0 |  641 | `	SyHashEntry *pEntry = SyHashGet(&pThis->hAttr,(const void *)zSlot,SyStrlen(zSlot));` |
|     ! 0 |  642 | `	if( pEntry ){` |
|     ! 0 |  643 | `		PH7_VmReleaseInstanceAttr(&(*pVm),(VmClassAttr *)pEntry->pUserData);` |
|     ! 0 |  644 | `		PH7_ClassInstanceDeleteAttrEntry(pThis,pEntry);` |
|     ! 0 |  645 | `	}` |
|     ! 0 |  646 | `}` |
|     ! 0 |  647 | `static int vm_builtin_Exception_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  648 | `{` |
|     ! 0 |  649 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - |  650 | `	ph7_value *pVal;` |
|     ! 0 |  651 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  652 | `	if( pThis == 0 ){` |
|     ! 0 |  653 | `		return PH7_OK;` |
|       - |  654 | `	}` |
|     ! 0 |  655 | `	pVal = PH7_NativeAttr(pThis,EXC_MESSAGE);` |
|     ! 0 |  656 | `	if( pVal && (pVal->iFlags & MEMOBJ_NULL) == 0 && (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 |  657 | `		VmExcDropSlot(pCtx->pVm,pThis,EXC_MESSAGE);` |
|     ! 0 |  658 | `	}` |
|     ! 0 |  659 | `	pVal = PH7_NativeAttr(pThis,EXC_CODE);` |
|     ! 0 |  660 | `	if( pVal && (pVal->iFlags & MEMOBJ_NULL) == 0 && (pVal->iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 |  661 | `		VmExcDropSlot(pCtx->pVm,pThis,EXC_CODE);` |
|     ! 0 |  662 | `	}` |
|     ! 0 |  663 | `	ph7_result_null(pCtx);` |
|     ! 0 |  664 | `	return PH7_OK;` |
|     ! 0 |  665 | `}` |
|       - |  666 | `/*` |
|       - |  667 | ` * php's smart_str_append_zval: a scalar or an enum case, the way a trace argument` |
|       - |  668 | `` * and an unhandled match case print one. A string is single-quoted, ESCAPED (`\n`,`` |
|       - |  669 | `` * `\\`, `\xNN` for anything outside printable ASCII) and cut after nMax bytes with`` |
|       - |  670 | `` * `...` inside the quotes -- nMax is zend.exception_string_param_max_len, so at 0`` |
|       - |  671 | `` * every non-empty string is `'...'` and the empty one `''`. A float takes php's`` |
|       - |  672 | `` * precision and always shows its fraction; an enum case prints `Enum::Case`.`` |
|       - |  673 | ` * Answers 0, appending nothing, for what php renders another way: an array, any` |
|       - |  674 | ` * other object, a resource.` |
|       - |  675 | ` */` |
|     262 |  676 | `PH7_PRIVATE int PH7_VmAppendTraceScalar(ph7_vm *pVm,SyBlob *pOut,ph7_value *pArg,sxi64 nMax)` |
|       3 |  677 | `{` |
|     265 |  678 | `	if( pArg == 0 \|\| (pArg->iFlags & MEMOBJ_NULL) ){` |
|      14 |  679 | `		SyBlobAppend(pOut,"NULL",sizeof("NULL")-1);` |
|      14 |  680 | `		return 1;` |
|       - |  681 | `	}` |
|     253 |  682 | `	if( pArg->iFlags & MEMOBJ_BOOL ){` |
|      24 |  683 | `		if( pArg->x.iVal ){` |
|      14 |  684 | `			SyBlobAppend(pOut,"true",sizeof("true")-1);` |
|       8 |  685 | `		}else{` |
|      12 |  686 | `			SyBlobAppend(pOut,"false",sizeof("false")-1);` |
|       - |  687 | `		}` |
|      24 |  688 | `		return 1;` |
|       - |  689 | `	}` |
|     231 |  690 | `	if( pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_RES) ){` |
|      17 |  691 | `		return 0;` |
|       - |  692 | `	}` |
|     217 |  693 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      27 |  694 | `		ph7_class_instance *pObj = (ph7_class_instance *)pArg->x.pOther;` |
|       - |  695 | `		ph7_value *pName;` |
|      27 |  696 | `		if( pObj == 0 \|\| pObj->pClass == 0 \|\| (pObj->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      17 |  697 | `			return 0;` |
|       - |  698 | `		}` |
|      11 |  699 | `		pName = PH7_NativeAttr(pObj,"name");` |
|      11 |  700 | `		SyBlobFormat(pOut,"%z::",&pObj->pClass->sDisp);` |
|      11 |  701 | `		if( pName ){` |
|      11 |  702 | `			SyBlobAppend(pOut,SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|       5 |  703 | `		}` |
|      11 |  704 | `		return 1;` |
|       - |  705 | `	}` |
|     193 |  706 | `	if( pArg->iFlags & MEMOBJ_STRING ){` |
|       - |  707 | `		static const char zHex[] = "0123456789ABCDEF";` |
|      91 |  708 | `		const unsigned char *z = (const unsigned char *)SyBlobData(&pArg->sBlob);` |
|      91 |  709 | `		sxu32 n = SyBlobLength(&pArg->sBlob);` |
|      91 |  710 | `		sxu32 nKeep = (nMax < 0 \|\| (sxu64)nMax >= n) ? n : (sxu32)nMax;` |
|       - |  711 | `		sxu32 i;` |
|      91 |  712 | `		SyBlobAppend(pOut,"'",1);` |
|     165 |  713 | `		for( i = 0 ; i < nKeep ; i++ ){` |
|      75 |  714 | `			unsigned char c = z[i];` |
|       - |  715 | `			char zEsc[4];` |
|      75 |  716 | `			if( c >= 32 && c <= 126 && c != '\\' ){` |
|      59 |  717 | `				SyBlobAppend(pOut,&z[i],1);` |
|      61 |  718 | `				continue;` |
|       - |  719 | `			}` |
|      17 |  720 | `			zEsc[0] = '\\';` |
|      17 |  721 | `			switch( c ){` |
|       5 |  722 | `			case '\n':  zEsc[1] = 'n';  break;` |
|     ! 0 |  723 | `			case '\r':  zEsc[1] = 'r';  break;` |
|       5 |  724 | `			case '\t':  zEsc[1] = 't';  break;` |
|     ! 0 |  725 | `			case '\f':  zEsc[1] = 'f';  break;` |
|     ! 0 |  726 | `			case '\v':  zEsc[1] = 'v';  break;` |
|       3 |  727 | `			case '\\': zEsc[1] = '\\'; break;` |
|       3 |  728 | `			case 27:    zEsc[1] = 'e';  break;` |
|       2 |  729 | `			default:` |
|       5 |  730 | `				zEsc[1] = 'x';` |
|       5 |  731 | `				zEsc[2] = zHex[c >> 4];` |
|       5 |  732 | `				zEsc[3] = zHex[c & 0xf];` |
|       5 |  733 | `				SyBlobAppend(pOut,zEsc,4);` |
|       5 |  734 | `				continue;` |
|       - |  735 | `			}` |
|      13 |  736 | `			SyBlobAppend(pOut,zEsc,2);` |
|       7 |  737 | `		}` |
|      91 |  738 | `		if( nKeep < n ){` |
|      77 |  739 | `			SyBlobAppend(pOut,"...",sizeof("...")-1);` |
|      37 |  740 | `		}` |
|      91 |  741 | `		SyBlobAppend(pOut,"'",1);` |
|      91 |  742 | `		return 1;` |
|       - |  743 | `	}` |
|       - |  744 | `	{` |
|       - |  745 | `		/* int / float: php prints the scalar itself -- but a trace FLOAT always` |
|       - |  746 | `		 * shows its fraction (1.0, not the "1" the ordinary string cast produces),` |
|       - |  747 | `		 * which is what tells a float argument apart from an int one. INF/NAN and` |
|       - |  748 | `		 * the exponent forms already carry a marker. */` |
|       - |  749 | `		ph7_value sTmp;` |
|       - |  750 | `		const char *z;` |
|       - |  751 | `		sxu32 n,i;` |
|     104 |  752 | `		int bMarked = 0;` |
|     104 |  753 | `		PH7_MemObjInit(&(*pVm),&sTmp);` |
|     104 |  754 | `		PH7_MemObjStore(pArg,&sTmp);` |
|     104 |  755 | `		PH7_MemObjToString(&sTmp);` |
|     104 |  756 | `		z = (const char *)SyBlobData(&sTmp.sBlob);` |
|     104 |  757 | `		n = SyBlobLength(&sTmp.sBlob);` |
|     104 |  758 | `		SyBlobAppend(pOut,z,n);` |
|     104 |  759 | `		if( pArg->iFlags & MEMOBJ_REAL ){` |
|     108 |  760 | `			for( i = 0 ; i < n ; ++i ){` |
|      96 |  761 | `				if( z[i] < '0' \|\| z[i] > '9' ){` |
|      54 |  762 | `					if( z[i] != '-' && z[i] != '+' ){` |
|      44 |  763 | `						bMarked = 1;` |
|      44 |  764 | `						break;` |
|       - |  765 | `					}` |
|       5 |  766 | `				}` |
|      28 |  767 | `			}` |
|      56 |  768 | `			if( !bMarked ){` |
|      14 |  769 | `				SyBlobAppend(pOut,".0",sizeof(".0")-1);` |
|       6 |  770 | `			}` |
|      27 |  771 | `		}` |
|     104 |  772 | `		PH7_MemObjRelease(&sTmp);` |
|       - |  773 | `	}` |
|     104 |  774 | `	return 1;` |
|     134 |  775 | `}` |
|       - |  776 | `/*` |
|       - |  777 | ` * One argument of a trace frame, php's _build_trace_args: the scalar shapes above,` |
|       - |  778 | `` * then `Array`, `Object(Class)` and `Resource id #N` for the rest.`` |
|       - |  779 | ` */` |
|     244 |  780 | `static void VmExcTraceArg(ph7_vm *pVm,SyBlob *pOut,ph7_value *pArg,sxi64 nMax)` |
|       3 |  781 | `{` |
|     247 |  782 | `	if( PH7_VmAppendTraceScalar(&(*pVm),pOut,pArg,nMax) ){` |
|     223 |  783 | `		return;` |
|       - |  784 | `	}` |
|      27 |  785 | `	if( pArg->iFlags & MEMOBJ_HASHMAP ){` |
|      15 |  786 | `		SyBlobAppend(pOut,"Array",sizeof("Array")-1);` |
|      21 |  787 | `	}else if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      15 |  788 | `		ph7_class_instance *pObj = (ph7_class_instance *)pArg->x.pOther;` |
|      15 |  789 | `		SyBlobAppend(pOut,"Object(",sizeof("Object(")-1);` |
|      15 |  790 | `		if( pObj && pObj->pClass ){` |
|      15 |  791 | `			SyBlobFormat(pOut,"%z",&pObj->pClass->sDisp);` |
|       6 |  792 | `		}` |
|      15 |  793 | `		SyBlobAppend(pOut,")",sizeof(")")-1);` |
|       9 |  794 | `	}else{` |
|       - |  795 | `		ph7_value sTmp;` |
|     ! 0 |  796 | `		PH7_MemObjInit(&(*pVm),&sTmp);` |
|     ! 0 |  797 | `		PH7_MemObjStore(pArg,&sTmp);` |
|     ! 0 |  798 | `		PH7_MemObjToString(&sTmp);` |
|     ! 0 |  799 | `		SyBlobAppend(pOut,SyBlobData(&sTmp.sBlob),SyBlobLength(&sTmp.sBlob));` |
|     ! 0 |  800 | `		PH7_MemObjRelease(&sTmp);` |
|       - |  801 | `	}` |
|     125 |  802 | `}` |
|       - |  803 | `/* An element of a trace frame, or NULL when the frame does not carry it. */` |
|    5406 |  804 | `static ph7_value * VmExcFrameField(ph7_vm *pVm,ph7_hashmap *pFrame,const char *zField)` |
|       5 |  805 | `{` |
|    5411 |  806 | `	ph7_hashmap_node *pNode = 0;` |
|       - |  807 | `	ph7_value sKey;` |
|       - |  808 | `	sxi32 rc;` |
|       - |  809 | `	SyString sName;` |
|    5411 |  810 | `	SyStringInitFromBuf(&sName,zField,SyStrlen(zField));` |
|    5411 |  811 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,&sName);` |
|    5411 |  812 | `	rc = PH7_HashmapLookup(pFrame,&sKey,&pNode);` |
|    5411 |  813 | `	PH7_MemObjRelease(&sKey);` |
|    5411 |  814 | `	if( rc != SXRET_OK \|\| pNode == 0 ){` |
|    2551 |  815 | `		return 0;` |
|       - |  816 | `	}` |
|    2865 |  817 | `	return (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|    2708 |  818 | `}` |
|    3590 |  819 | `static void VmExcFrameStr(SyBlob *pOut,ph7_value *pVal)` |
|       5 |  820 | `{` |
|    3595 |  821 | `	if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){` |
|    1919 |  822 | `		SyBlobAppend(pOut,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|     957 |  823 | `	}` |
|    3595 |  824 | `}` |
|       - |  825 | `/* A slot's string form, taken through a COPY: converting the value in place` |
|       - |  826 | ` * would rewrite the exception's own state. */` |
|      12 |  827 | `static void VmExcValueStr(ph7_vm *pVm,ph7_value *pVal,SyBlob *pOut)` |
|       3 |  828 | `{` |
|       - |  829 | `	ph7_value sTmp;` |
|      15 |  830 | `	if( pVal == 0 ){` |
|     ! 0 |  831 | `		return;` |
|       - |  832 | `	}` |
|      15 |  833 | `	PH7_MemObjInit(&(*pVm),&sTmp);` |
|      15 |  834 | `	PH7_MemObjStore(pVal,&sTmp);` |
|      15 |  835 | `	PH7_MemObjToString(&sTmp);` |
|      15 |  836 | `	SyBlobAppend(pOut,SyBlobData(&sTmp.sBlob),SyBlobLength(&sTmp.sBlob));` |
|      15 |  837 | `	PH7_MemObjRelease(&sTmp);` |
|       9 |  838 | `}` |
|       - |  839 | ``/* Does the blob contain this literal? SyBlobSearch() is `#ifndef`` |
|       - |  840 | `` * PH7_DISABLE_BUILTIN_FUNC`, and the exception family exists in the tiny build`` |
|       - |  841 | ` * too, so the one search this file needs is spelled out. */` |
|     ! 0 |  842 | `static int VmExcBlobHas(SyBlob *pBlob,const char *zPat,sxu32 nPat)` |
|     ! 0 |  843 | `{` |
|     ! 0 |  844 | `	const char *z = (const char *)SyBlobData(pBlob);` |
|     ! 0 |  845 | `	sxu32 n = SyBlobLength(pBlob);` |
|       - |  846 | `	sxu32 i;` |
|     ! 0 |  847 | `	if( nPat == 0 \|\| n < nPat ){` |
|     ! 0 |  848 | `		return 0;` |
|       - |  849 | `	}` |
|     ! 0 |  850 | `	for( i = 0 ; i + nPat <= n ; i++ ){` |
|     ! 0 |  851 | `		if( SyMemcmp((const void *)&z[i],(const void *)zPat,nPat) == 0 ){` |
|     ! 0 |  852 | `			return 1;` |
|       - |  853 | `		}` |
|     ! 0 |  854 | `	}` |
|     ! 0 |  855 | `	return 0;` |
|     ! 0 |  856 | `}` |
|       - |  857 | ``/* php's `Z_OBJCE_P == zend_ce_type_error \|\| == zend_ce_argument_count_error`:`` |
|       - |  858 | ` * the two classes whose message __toString finishes with " and defined". */` |
|      12 |  859 | `static int VmExcIsArgError(ph7_vm *pVm,ph7_class_instance *pExc)` |
|       3 |  860 | `{` |
|      15 |  861 | `	ph7_class *pClass = pExc ? pExc->pClass : 0;` |
|       - |  862 | `	ph7_class *pType;` |
|      15 |  863 | `	if( pClass == 0 ){` |
|     ! 0 |  864 | `		return 0;` |
|       - |  865 | `	}` |
|      15 |  866 | `	pType = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,FALSE,0);` |
|      15 |  867 | `	if( pType && pClass == pType ){` |
|     ! 0 |  868 | `		return 1;` |
|       - |  869 | `	}` |
|      15 |  870 | `	pType = PH7_VmExtractClass(&(*pVm),"ArgumentCountError",sizeof("ArgumentCountError")-1,FALSE,0);` |
|      15 |  871 | `	return pType != 0 && pClass == pType;` |
|       9 |  872 | `}` |
|       - |  873 | `/*` |
|       - |  874 | `` * php's zend_trace_to_string: one `#N file(line): Class->method(args)` line per`` |
|       - |  875 | `` * frame, then `#N {main}` with NO trailing newline. A frame with no `file` is`` |
|       - |  876 | `` * php's `[internal function]: `.`` |
|       - |  877 | ` */` |
|     826 |  878 | `PH7_PRIVATE void PH7_VmTraceToString(ph7_vm *pVm,ph7_value *pTrace,int bMainMarker,SyBlob *pOut)` |
|       5 |  879 | `{` |
|       - |  880 | `	ph7_hashmap *pMap;` |
|       - |  881 | `	ph7_hashmap_node *pEntry;` |
|     831 |  882 | `	sxu32 nFrame = 0;` |
|     831 |  883 | `	sxi64 nMax = PH7_VmIniGetInt(&(*pVm),"zend.exception_string_param_max_len",0);` |
|     831 |  884 | `	if( pTrace && (pTrace->iFlags & MEMOBJ_HASHMAP) && pTrace->x.pOther ){` |
|     831 |  885 | `		pMap = (ph7_hashmap *)pTrace->x.pOther;` |
|       - |  886 | `		/* Insertion order is pFirst then the pPrev chain (rule 12). */` |
|    1745 |  887 | `		for( pEntry = pMap->pFirst ; pEntry ; pEntry = pEntry->pPrev ){` |
|     919 |  888 | `			ph7_value *pFrameVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pEntry->nValIdx);` |
|       - |  889 | `			ph7_hashmap *pFrame;` |
|       - |  890 | `			ph7_value *pFile;` |
|     919 |  891 | `			if( pFrameVal == 0 \|\| (pFrameVal->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 |  892 | `				continue;` |
|       - |  893 | `			}` |
|     919 |  894 | `			pFrame = (ph7_hashmap *)pFrameVal->x.pOther;` |
|     919 |  895 | `			SyBlobFormat(pOut,"#%u ",nFrame);` |
|     919 |  896 | `			pFile = VmExcFrameField(&(*pVm),pFrame,"file");` |
|    1337 |  897 | `			if( pFile && (pFile->iFlags & MEMOBJ_STRING) ){` |
|     841 |  898 | `				ph7_value *pLine = VmExcFrameField(&(*pVm),pFrame,"line");` |
|     841 |  899 | `				VmExcFrameStr(pOut,pFile);` |
|    1677 |  900 | `				SyBlobFormat(pOut,"(%qd): ",` |
|     836 |  901 | `					(pLine && (pLine->iFlags & MEMOBJ_INT)) ? pLine->x.iVal : (sxi64)0);` |
|     423 |  902 | `			}else{` |
|      82 |  903 | `				SyBlobAppend(pOut,"[internal function]: ",sizeof("[internal function]: ")-1);` |
|       - |  904 | `			}` |
|     919 |  905 | `			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"class"));` |
|     919 |  906 | `			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"type"));` |
|     919 |  907 | `			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"function"));` |
|     919 |  908 | `			SyBlobAppend(pOut,"(",sizeof("(")-1);` |
|       - |  909 | `			{` |
|     919 |  910 | `				ph7_value *pArgs = VmExcFrameField(&(*pVm),pFrame,"args");` |
|     919 |  911 | `				if( pArgs && (pArgs->iFlags & MEMOBJ_HASHMAP) && pArgs->x.pOther ){` |
|     125 |  912 | `					ph7_hashmap *pArgMap = (ph7_hashmap *)pArgs->x.pOther;` |
|       - |  913 | `					ph7_hashmap_node *pArg;` |
|     125 |  914 | `					int bFirst = 1;` |
|     369 |  915 | `					for( pArg = pArgMap->pFirst ; pArg ; pArg = pArg->pPrev ){` |
|     247 |  916 | `						if( !bFirst ){` |
|     135 |  917 | `							SyBlobAppend(pOut,", ",sizeof(", ")-1);` |
|      66 |  918 | `						}` |
|     247 |  919 | `						bFirst = 0;` |
|     369 |  920 | `						VmExcTraceArg(&(*pVm),pOut,` |
|     122 |  921 | `							(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pArg->nValIdx),nMax);` |
|     125 |  922 | `					}` |
|      61 |  923 | `				}` |
|       - |  924 | `			}` |
|     919 |  925 | `			SyBlobAppend(pOut,")\n",sizeof(")\n")-1);` |
|     919 |  926 | `			nFrame++;` |
|     462 |  927 | `		}` |
|     413 |  928 | `	}` |
|     831 |  929 | `	if( bMainMarker ){` |
|       - |  930 | `		/* getTraceAsString() ends on the bottom marker; debug_print_backtrace()` |
|       - |  931 | `		 * does not print one -- it stops after the last real frame. */` |
|     787 |  932 | `		SyBlobFormat(pOut,"#%u {main}",nFrame);` |
|     391 |  933 | `	}` |
|     831 |  934 | `}` |
|     724 |  935 | `static int vm_builtin_Exception_getTraceAsString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  936 | `{` |
|     729 |  937 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - |  938 | `	SyBlob sOut;` |
|     362 |  939 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     729 |  940 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     729 |  941 | `	PH7_VmTraceToString(pCtx->pVm,pThis ? PH7_NativeAttr(pThis,EXC_TRACE) : 0,TRUE,&sOut);` |
|     729 |  942 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     729 |  943 | `	SyBlobRelease(&sOut);` |
|     729 |  944 | `	return PH7_OK;` |
|       5 |  945 | `}` |
|       - |  946 | `/*` |
|       - |  947 | ` * php's Exception::__toString.` |
|       - |  948 | ` *` |
|       - |  949 | ` *    C: message in file:line` |
|       - |  950 | ` *    Stack trace:` |
|       - |  951 | ` *    <trace>` |
|       - |  952 | ` *` |
|       - |  953 | ` * The PREVIOUS chain is part of the format and the ORDER is inverted: php builds` |
|       - |  954 | `` * the string innermost-first and joins the shallower ones after `\n\nNext `, so`` |
|       - |  955 | ` * the root cause is printed first. The chunk answered a four-field space-joined` |
|       - |  956 | `` * line instead — `file line code message` — which no php ever produced, and it is`` |
|       - |  957 | `` * what an uncaught exception, `echo $e` and `(string)$e` all show.`` |
|       - |  958 | ` *` |
|       - |  959 | ` * The walk carries its ancestors on the C stack (rule 31): php protects each` |
|       - |  960 | `` * object it visits and stops when it comes back round, and a `$a->previous = $b;`` |
|       - |  961 | `` * $b->previous = $a` pair must not spin.`` |
|       - |  962 | ` */` |
|       - |  963 | `#define EXC_CHAIN_MAX 256` |
|       8 |  964 | `static int vm_builtin_Exception_toString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  965 | `{` |
|       - |  966 | `	ph7_class_instance *apChain[EXC_CHAIN_MAX];` |
|      11 |  967 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      11 |  968 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - |  969 | `	SyBlob sOut;` |
|      11 |  970 | `	int nChain = 0;` |
|       - |  971 | `	int i,j;` |
|       4 |  972 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      23 |  973 | `	while( pThis && nChain < EXC_CHAIN_MAX ){` |
|       - |  974 | `		ph7_class_instance *pPrev;` |
|      19 |  975 | `		for( j = 0 ; j < nChain ; j++ ){` |
|       6 |  976 | `			if( apChain[j] == pThis ){` |
|     ! 0 |  977 | `				pThis = 0;    /* already on the chain: php's recursion protection */` |
|     ! 0 |  978 | `				break;` |
|       - |  979 | `			}` |
|       4 |  980 | `		}` |
|      15 |  981 | `		if( pThis == 0 ){` |
|     ! 0 |  982 | `			break;` |
|       - |  983 | `		}` |
|      15 |  984 | `		apChain[nChain++] = pThis;` |
|      15 |  985 | `		pPrev = PH7_NativeAttrObj(pThis,EXC_PREVIOUS);` |
|      15 |  986 | `		pThis = pPrev;` |
|       3 |  987 | `	}` |
|       - |  988 | `	/* php formats the SHALLOWEST first and pushes each one it has already built` |
|       - |  989 | `	 * behind the next, so the printed order is inverted: the ROOT CAUSE leads and` |
|       - |  990 | ``	 * every caller follows it after `\n\nNext `. */`` |
|      11 |  991 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      23 |  992 | `	for( i = 0 ; i < nChain ; i++ ){` |
|      15 |  993 | `		ph7_class_instance *pExc = apChain[i];` |
|      15 |  994 | `		ph7_value *pLine = PH7_NativeAttr(pExc,EXC_LINE);` |
|       - |  995 | `		SyBlob sMsg;` |
|       - |  996 | `		SyBlob sThis;` |
|      15 |  997 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      15 |  998 | `		SyBlobInit(&sThis,&pVm->sAllocator);` |
|      15 |  999 | `		VmExcValueStr(pVm,PH7_NativeAttr(pExc,EXC_MESSAGE),&sMsg);` |
|       - | 1000 | `		/* php's one message rewrite: a TypeError/ArgumentCountError raised at a` |
|       - | 1001 | `		 * CALL SITE says "..., called in F on line N", and __toString finishes the` |
|       - | 1002 | `		 * sentence with " and defined". */` |
|      12 | 1003 | `		if( VmExcIsArgError(pVm,pExc)` |
|       9 | 1004 | `		 && VmExcBlobHas(&sMsg,", called in ",sizeof(", called in ")-1) ){` |
|     ! 0 | 1005 | `			SyBlobAppend(&sMsg," and defined",sizeof(" and defined")-1);` |
|     ! 0 | 1006 | `		}` |
|      15 | 1007 | `		SyBlobFormat(&sThis,"%z",&pExc->pClass->sDisp);` |
|      15 | 1008 | `		if( SyBlobLength(&sMsg) > 0 ){` |
|      13 | 1009 | `			SyBlobAppend(&sThis,": ",sizeof(": ")-1);` |
|      13 | 1010 | `			SyBlobAppend(&sThis,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       5 | 1011 | `		}` |
|      15 | 1012 | `		SyBlobAppend(&sThis," in ",sizeof(" in ")-1);` |
|      15 | 1013 | `		VmExcFrameStr(&sThis,PH7_NativeAttr(pExc,EXC_FILE));` |
|      21 | 1014 | `		SyBlobFormat(&sThis,":%qd\nStack trace:\n",` |
|      12 | 1015 | `			(pLine && (pLine->iFlags & MEMOBJ_INT)) ? pLine->x.iVal : (sxi64)0);` |
|      15 | 1016 | `		PH7_VmTraceToString(pVm,PH7_NativeAttr(pExc,EXC_TRACE),TRUE,&sThis);` |
|      15 | 1017 | `		if( SyBlobLength(&sOut) > 0 ){` |
|       6 | 1018 | `			SyBlobAppend(&sThis,"\n\nNext ",sizeof("\n\nNext ")-1);` |
|       6 | 1019 | `			SyBlobAppend(&sThis,SyBlobData(&sOut),SyBlobLength(&sOut));` |
|       2 | 1020 | `		}` |
|      15 | 1021 | `		SyBlobReset(&sOut);` |
|      15 | 1022 | `		SyBlobAppend(&sOut,SyBlobData(&sThis),SyBlobLength(&sThis));` |
|      15 | 1023 | `		SyBlobRelease(&sMsg);` |
|      15 | 1024 | `		SyBlobRelease(&sThis);` |
|       9 | 1025 | `	}` |
|      11 | 1026 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|      11 | 1027 | `	SyBlobRelease(&sOut);` |
|      11 | 1028 | `	return PH7_OK;` |
|       3 | 1029 | `}` |
|       - | 1030 | `/*` |
|       - | 1031 | ` * The declaration. php's two roots carry the same eleven methods and the same` |
|       - | 1032 | `` * seven slots; the only difference php's stub records is `Error::$line`, which`` |
|       - | 1033 | ` * has NO default where Exception's is 0.` |
|       - | 1034 | ` *` |
|       - | 1035 | `` * PH7_CLASS_NOCLONE on EVERY row: php refuses `clone $e` outright, and a native`` |
|       - | 1036 | ` * subclass does not inherit its parent's class flags (rule 29).` |
|       - | 1037 | ` */` |
|       - | 1038 | `#define EXC_METHODS(zCtor,xCtor) \` |
|       - | 1039 | `	{ "__clone",          PH7_MOD_PRIVATE, "", "void", vm_builtin_Exception_clone }, \` |
|       - | 1040 | `	{ "__construct",      PH7_MOD_PUBLIC, zCtor, 0, xCtor }, \` |
|       - | 1041 | `	{ "__wakeup",         PH7_MOD_PUBLIC, "", "@void", vm_builtin_Exception_wakeup }, \` |
|       - | 1042 | `	{ "getMessage",       PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "string", \` |
|       - | 1043 | `	  vm_builtin_Exception_getMessage }, \` |
|       - | 1044 | `	{ "getCode",          PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", 0, \` |
|       - | 1045 | `	  vm_builtin_Exception_getCode }, \` |
|       - | 1046 | `	{ "getFile",          PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "string", \` |
|       - | 1047 | `	  vm_builtin_Exception_getFile }, \` |
|       - | 1048 | `	{ "getLine",          PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "int", \` |
|       - | 1049 | `	  vm_builtin_Exception_getLine }, \` |
|       - | 1050 | `	{ "getTrace",         PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "array", \` |
|       - | 1051 | `	  vm_builtin_Exception_getTrace }, \` |
|       - | 1052 | `	{ "getPrevious",      PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "?Throwable", \` |
|       - | 1053 | `	  vm_builtin_Exception_getPrevious }, \` |
|       - | 1054 | `	{ "getTraceAsString", PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "string", \` |
|       - | 1055 | `	  vm_builtin_Exception_getTraceAsString }, \` |
|       - | 1056 | `	{ "__toString",       PH7_MOD_PUBLIC, "", "string", vm_builtin_Exception_toString }` |
|       - | 1057 | `#define EXC_CTOR_SIG "string $message = \"\", int $code = 0, ?Throwable $previous = null"` |
|       - | 1058 | `/* php's seven slots, twice: the only difference between the two roots is` |
|       - | 1059 | `` * `Error::$line`, which php's stub declares with NO default where Exception's is`` |
|       - | 1060 | `` * 0 (`PH7_NATIVE_VAL_NONE` — its hasDefaultValue() is false and the export`` |
|       - | 1061 | `` * prints `protected int $line` bare). `message` and `code` are the two php leaves`` |
|       - | 1062 | ` * UNTYPED, and its stub says why: BC, since a subclass may have assigned` |
|       - | 1063 | ` * anything to them. */` |
|       - | 1064 | `#define EXC_PROP_HEAD \` |
|       - | 1065 | `	{ EXC_MESSAGE,  PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 }, \` |
|       - | 1066 | `	{ EXC_STRING,   PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" }, \` |
|       - | 1067 | `	{ EXC_CODE,     PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 }, \` |
|       - | 1068 | `	{ EXC_FILE,     PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" }` |
|       - | 1069 | `#define EXC_PROP_TAIL \` |
|       - | 1070 | `	{ EXC_TRACE,    PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_ARRAY, 0, 0, 0.0 }, "array" }, \` |
|       - | 1071 | `	{ EXC_PREVIOUS, PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?Throwable" }` |
|    8445 | 1072 | `static sxi32 VmInstallExceptions(ph7_vm *pVm)` |
|       5 | 1073 | `{` |
|       - | 1074 | `	static const PH7_NativeMethodDef aExcMethod[] = {` |
|       - | 1075 | `		EXC_METHODS(EXC_CTOR_SIG,vm_builtin_Exception_construct)` |
|       - | 1076 | `	};` |
|       - | 1077 | `	static const PH7_NativePropDef aExcProp[] = {` |
|       - | 1078 | `		EXC_PROP_HEAD,` |
|       - | 1079 | `		{ EXC_LINE, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, "int" },` |
|       - | 1080 | `		EXC_PROP_TAIL` |
|       - | 1081 | `	};` |
|       - | 1082 | `	static const PH7_NativePropDef aErrProp[] = {` |
|       - | 1083 | `		EXC_PROP_HEAD,` |
|       - | 1084 | `		{ EXC_LINE, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|       - | 1085 | `		EXC_PROP_TAIL` |
|       - | 1086 | `	};` |
|       - | 1087 | `	static const PH7_NativePropDef aErrExcProp[] = {` |
|       - | 1088 | `		{ EXC_SEVERITY, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 1, 0, 0.0 }, "int" },` |
|       - | 1089 | `	};` |
|       - | 1090 | `	static const PH7_NativeMethodDef aErrExcMethod[] = {` |
|       - | 1091 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|       - | 1092 | `		  "string $message = \"\", int $code = 0, int $severity = E_ERROR, "` |
|       - | 1093 | `		  "?string $filename = null, ?int $line = null, ?Throwable $previous = null", 0,` |
|       - | 1094 | `		  vm_builtin_ErrorException_construct },` |
|       - | 1095 | `		{ "getSeverity", PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "int",` |
|       - | 1096 | `		  vm_builtin_ErrorException_getSeverity },` |
|       - | 1097 | `	};` |
|       - | 1098 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1099 | `		{ "Exception", 0, "Throwable", PH7_CLASS_NOCLONE,` |
|       - | 1100 | `		  aExcMethod, SX_ARRAYSIZE(aExcMethod), 0, 0, aExcProp, SX_ARRAYSIZE(aExcProp), 0, 0, 0 },` |
|       - | 1101 | `		{ "Error", 0, "Throwable", PH7_CLASS_NOCLONE,` |
|       - | 1102 | `		  aExcMethod, SX_ARRAYSIZE(aExcMethod), 0, 0, aErrProp, SX_ARRAYSIZE(aErrProp), 0, 0, 0 },` |
|       - | 1103 | `		/* Zend's own subclasses, then ErrorException, then SPL's tree. Every row is` |
|       - | 1104 | `		 * declaration-only in php too. */` |
|       - | 1105 | `		{ "TypeError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1106 | `		{ "ArgumentCountError", "TypeError", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1107 | `		{ "ValueError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1108 | `		{ "FiberError", "Error", 0,` |
|       - | 1109 | `		  PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1110 | `		{ "AssertionError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1111 | `		{ "ArithmeticError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1112 | `		{ "DivisionByZeroError", "ArithmeticError", 0, PH7_CLASS_NOCLONE,` |
|       - | 1113 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1114 | `		{ "UnhandledMatchError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1115 | `		{ "CompileError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1116 | `		{ "ParseError", "CompileError", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1117 | `		{ "ErrorException", "Exception", 0, PH7_CLASS_NOCLONE,` |
|       - | 1118 | `		  aErrExcMethod, SX_ARRAYSIZE(aErrExcMethod), 0, 0,` |
|       - | 1119 | `		  aErrExcProp, SX_ARRAYSIZE(aErrExcProp), 0, 0, 0 },` |
|       - | 1120 | `		{ "LogicException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1121 | `		{ "RuntimeException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1122 | `		{ "BadFunctionCallException", "LogicException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1123 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1124 | `		{ "BadMethodCallException", "BadFunctionCallException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1125 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1126 | `		{ "DomainException", "LogicException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1127 | `		{ "InvalidArgumentException", "LogicException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1128 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1129 | `		{ "LengthException", "LogicException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1130 | `		{ "OutOfRangeException", "LogicException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1131 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1132 | `		{ "OutOfBoundsException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1133 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1134 | `		{ "OverflowException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1135 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1136 | `		{ "RangeException", "RuntimeException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1137 | `		{ "UnderflowException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1138 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1139 | `		{ "UnexpectedValueException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1140 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1141 | `		{ "JsonException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1142 | `	};` |
|       - | 1143 | `	{` |
|    8450 | 1144 | `		sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|    8450 | 1145 | `		if( rc == SXRET_OK ){` |
|       - | 1146 | ``			/* php refuses `new FiberError` -- the engine is the only thing that`` |
|       - | 1147 | `			 * raises one -- and words the refusal per class. */` |
|    8450 | 1148 | `			ph7_class *pFe = PH7_VmExtractClass(&(*pVm),"FiberError",` |
|       - | 1149 | `				sizeof("FiberError")-1,FALSE,0);` |
|    8450 | 1150 | `			if( pFe ){` |
|    8450 | 1151 | `				pFe->zNewRefusal = "The \"FiberError\" class is reserved for internal use "` |
|       - | 1152 | `					"and cannot be manually instantiated";` |
|    4217 | 1153 | `			}` |
|    4217 | 1154 | `		}` |
|    8450 | 1155 | `		return rc;` |
|       - | 1156 | `	}` |
|       5 | 1157 | `}` |
|       - | 1158 | `/*` |
|       - | 1159 | ` * The eleven core interfaces, declared from C.` |
|       - | 1160 | ` *` |
|       - | 1161 | ` * They are contracts -- no method here has a body, every row is` |
|       - | 1162 | ` * PH7_MOD_ABSTRACT -- so the conversion is entirely about what the DECLARATION` |
|       - | 1163 | ` * says, which is where a chunk fell short in four php-visible ways:` |
|       - | 1164 | ` *` |
|       - | 1165 | `` *  - php's `interface Throwable extends Stringable`: the chunk redeclared`` |
|       - | 1166 | ` *    __toString() on Throwable instead, so no Exception was ever Stringable` |
|       - | 1167 | `` *    (`$e instanceof Stringable` was false, and Reflection attributed the`` |
|       - | 1168 | `` *    method to Throwable rather than printing php's `inherits Stringable`);`` |
|       - | 1169 | ` *  - php declares a RETURN TYPE on all but three of these methods and marks` |
|       - | 1170 | `` *    nearly all of them TENTATIVE (the leading `@`, rule 45) -- a chunk has no`` |
|       - | 1171 | ` *    way to say tentative at all;` |
|       - | 1172 | `` *  - php's `mixed` on ArrayAccess's offsets, which the chunk left untyped;`` |
|       - | 1173 | ` *  - method ORDER, which Reflection prints: php lists Throwable's getPrevious` |
|       - | 1174 | ` *    before getTraceAsString, and Iterator's as current/next/key/valid/rewind.` |
|       - | 1175 | ` *` |
|       - | 1176 | ` * Order within the table is php's stub order too; the declare-then-link phases` |
|       - | 1177 | ` * of PH7_InstallNativeClasses let Throwable name Stringable and Iterator name` |
|       - | 1178 | `` * Traversable regardless of row order. An interface's parent is `zParent`, not`` |
|       - | 1179 | `` * `zImplements` (Reflection walks pBase to attribute an inherited method).`` |
|       - | 1180 | ` */` |
|    8445 | 1181 | `static sxi32 VmInstallCoreInterfaces(ph7_vm *pVm)` |
|       5 | 1182 | `{` |
|       - | 1183 | `	static const PH7_NativeMethodDef aStringable[] = {` |
|       - | 1184 | `		{ "__toString", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1185 | `	};` |
|       - | 1186 | `	static const PH7_NativeMethodDef aThrowable[] = {` |
|       - | 1187 | `		/* Not one of these is tentative: php's Throwable is a real contract. */` |
|       - | 1188 | `		{ "getMessage",       PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1189 | `		{ "getCode",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|       - | 1190 | `		{ "getFile",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1191 | `		{ "getLine",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "int", 0 },` |
|       - | 1192 | `		{ "getTrace",         PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "array", 0 },` |
|       - | 1193 | `		{ "getPrevious",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "?Throwable", 0 },` |
|       - | 1194 | `		{ "getTraceAsString", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1195 | `	};` |
|       - | 1196 | `	static const PH7_NativeMethodDef aArrayAccess[] = {` |
|       - | 1197 | `		{ "offsetExists", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset", "@bool", 0 },` |
|       - | 1198 | `		{ "offsetGet",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset", "@mixed", 0 },` |
|       - | 1199 | `		{ "offsetSet",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset, mixed $value",` |
|       - | 1200 | `		  "@void", 0 },` |
|       - | 1201 | `		{ "offsetUnset",  PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset", "@void", 0 },` |
|       - | 1202 | `	};` |
|       - | 1203 | `	static const PH7_NativeMethodDef aCountable[] = {` |
|       - | 1204 | `		{ "count", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@int", 0 },` |
|       - | 1205 | `	};` |
|       - | 1206 | `	static const PH7_NativeMethodDef aJsonSerializable[] = {` |
|       - | 1207 | `		{ "jsonSerialize", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@mixed", 0 },` |
|       - | 1208 | `	};` |
|       - | 1209 | `	/* The concrete cases()/from()/tryFrom() an enum gets are native methods` |
|       - | 1210 | `	 * declared to match these (oo_native.c, PH7_InstallEnumInterfaceMethods). */` |
|       - | 1211 | `	static const PH7_NativeMethodDef aUnitEnum[] = {` |
|       - | 1212 | `		{ "cases", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_ABSTRACT, "", "array", 0 },` |
|       - | 1213 | `	};` |
|       - | 1214 | `	static const PH7_NativeMethodDef aBackedEnum[] = {` |
|       - | 1215 | `		{ "from",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_ABSTRACT, "string\|int $value",` |
|       - | 1216 | `		  "static", 0 },` |
|       - | 1217 | `		{ "tryFrom", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_ABSTRACT, "string\|int $value",` |
|       - | 1218 | `		  "?static", 0 },` |
|       - | 1219 | `	};` |
|       - | 1220 | `	static const PH7_NativeMethodDef aIterator[] = {` |
|       - | 1221 | `		{ "current", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@mixed", 0 },` |
|       - | 1222 | `		{ "next",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@void", 0 },` |
|       - | 1223 | `		{ "key",     PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@mixed", 0 },` |
|       - | 1224 | `		{ "valid",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@bool", 0 },` |
|       - | 1225 | `		{ "rewind",  PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@void", 0 },` |
|       - | 1226 | `	};` |
|       - | 1227 | `	static const PH7_NativeMethodDef aIteratorAggregate[] = {` |
|       - | 1228 | `		{ "getIterator", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@Traversable", 0 },` |
|       - | 1229 | `	};` |
|       - | 1230 | `	/* php's legacy Serializable declares NO return type on either method. */` |
|       - | 1231 | `	static const PH7_NativeMethodDef aSerializable[] = {` |
|       - | 1232 | `		{ "serialize",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|       - | 1233 | `		{ "unserialize", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $data", 0, 0 },` |
|       - | 1234 | `	};` |
|       - | 1235 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1236 | `		{ "Traversable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1237 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1238 | `		{ "Stringable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1239 | `		  aStringable, SX_ARRAYSIZE(aStringable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1240 | `		{ "Throwable", "Stringable", 0, PH7_CLASS_INTERFACE,` |
|       - | 1241 | `		  aThrowable, SX_ARRAYSIZE(aThrowable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1242 | `		{ "ArrayAccess", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1243 | `		  aArrayAccess, SX_ARRAYSIZE(aArrayAccess), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1244 | `		{ "Countable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1245 | `		  aCountable, SX_ARRAYSIZE(aCountable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1246 | `		{ "JsonSerializable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1247 | `		  aJsonSerializable, SX_ARRAYSIZE(aJsonSerializable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1248 | `		{ "UnitEnum", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1249 | `		  aUnitEnum, SX_ARRAYSIZE(aUnitEnum), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1250 | `		{ "BackedEnum", "UnitEnum", 0, PH7_CLASS_INTERFACE,` |
|       - | 1251 | `		  aBackedEnum, SX_ARRAYSIZE(aBackedEnum), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1252 | `		{ "Iterator", "Traversable", 0, PH7_CLASS_INTERFACE,` |
|       - | 1253 | `		  aIterator, SX_ARRAYSIZE(aIterator), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1254 | `		{ "IteratorAggregate", "Traversable", 0, PH7_CLASS_INTERFACE,` |
|       - | 1255 | `		  aIteratorAggregate, SX_ARRAYSIZE(aIteratorAggregate), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1256 | `		{ "Serializable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1257 | `		  aSerializable, SX_ARRAYSIZE(aSerializable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1258 | `	};` |
|    8450 | 1259 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       5 | 1260 | `}` |
|       - | 1261 | `/*` |
|       - | 1262 | ` * ---------------------------------------------------------------------------` |
|       - | 1263 | `` * php's Directory — the object `dir()` answers.`` |
|       - | 1264 | ` *` |
|       - | 1265 | ` * php declares it FINAL with **no constructor at all**: the class is created by` |
|       - | 1266 | `` * `dir()` and `new Directory` is refused in the create_object handler, with a`` |
|       - | 1267 | ` * sentence that names dir() as the way to get one. Its two slots are` |
|       - | 1268 | `` * `public protected(set) readonly`, so a script can read `$d->path` and never`` |
|       - | 1269 | `` * write it, and its three methods declare return types (`read(): string\|false`).`` |
|       - | 1270 | ` * The chunk had a public constructor, a __destruct php does not declare, no` |
|       - | 1271 | ` * types anywhere and writable slots.` |
|       - | 1272 | ` * ---------------------------------------------------------------------------` |
|       - | 1273 | ` */` |
|       - | 1274 | `#define DIR_HANDLE "handle"` |
|       - | 1275 | `#define DIR_PATH   "path"` |
|       - | 1276 | `/*` |
|       - | 1277 | ` * Forward one method to the engine's own directory builtin (rule 7: call, don't` |
|       - | 1278 | `` * reimplement). php's Directory methods are `php_stream_readdir(...)` on the very`` |
|       - | 1279 | `` * stream `readdir()` uses, and a CLOSED handle is a TypeError there — the one`` |
|       - | 1280 | ` * place php's wording names the class rather than the function.` |
|       - | 1281 | ` */` |
|      40 | 1282 | `static int VmDirClosed(ph7_value *pHandle)` |
|       2 | 1283 | `{` |
|      42 | 1284 | `	io_private *pDev = (io_private *)pHandle->x.pOther;` |
|      42 | 1285 | `	return IO_PRIVATE_INVALID(pDev);` |
|       2 | 1286 | `}` |
|      40 | 1287 | `static int VmDirForward(ph7_context *pCtx,const char *zFunc,const char *zMethod)` |
|       2 | 1288 | `{` |
|      42 | 1289 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      42 | 1290 | `	ph7_value *pHandle = pThis ? PH7_NativeAttr(pThis,DIR_HANDLE) : 0;` |
|       - | 1291 | `	ph7_value *apArg[1];` |
|       - | 1292 | `	ph7_value sResult;` |
|       - | 1293 | `	ph7_value sName;` |
|       - | 1294 | `	SyString sStr;` |
|       - | 1295 | `	sxi32 rc;` |
|       - | 1296 | ``	/* php's check is `php_stream_from_zval` on a stream it CLOSED: closedir()`` |
|       - | 1297 | `	 * keeps the resource alive and marks it (gettype() answers` |
|       - | 1298 | `	 * "resource (closed)"), so the test is the magic, not the type. */` |
|      40 | 1299 | `	if( pHandle == 0 \|\| (pHandle->iFlags & MEMOBJ_RES) == 0` |
|      42 | 1300 | `	 \|\| VmDirClosed(pHandle) ){` |
|      10 | 1301 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1302 | `			"Directory::%s(): cannot use Directory resource after it has been closed",` |
|       3 | 1303 | `			zMethod);` |
|       - | 1304 | `	}` |
|      36 | 1305 | `	SyStringInitFromBuf(&sStr,zFunc,SyStrlen(zFunc));` |
|      36 | 1306 | `	PH7_MemObjInit(pCtx->pVm,&sName);` |
|      36 | 1307 | `	PH7_MemObjInitFromString(pCtx->pVm,&sName,&sStr);` |
|      36 | 1308 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      36 | 1309 | `	apArg[0] = pHandle;` |
|      36 | 1310 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,&sName,1,apArg,&sResult);` |
|      36 | 1311 | `	PH7_MemObjRelease(&sName);` |
|      36 | 1312 | `	if( rc == SXRET_OK ){` |
|      36 | 1313 | `		ph7_result_value(pCtx,&sResult);` |
|      17 | 1314 | `	}` |
|      36 | 1315 | `	PH7_MemObjRelease(&sResult);` |
|      36 | 1316 | `	return PH7_OK;` |
|      22 | 1317 | `}` |
|      28 | 1318 | `static int vm_builtin_Directory_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1319 | `{` |
|      14 | 1320 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      30 | 1321 | `	return VmDirForward(pCtx,"readdir","read");` |
|       2 | 1322 | `}` |
|       4 | 1323 | `static int vm_builtin_Directory_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1324 | `{` |
|       2 | 1325 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       5 | 1326 | `	return VmDirForward(pCtx,"rewinddir","rewind");` |
|       1 | 1327 | `}` |
|       8 | 1328 | `static int vm_builtin_Directory_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1329 | `{` |
|       4 | 1330 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      10 | 1331 | `	return VmDirForward(pCtx,"closedir","close");` |
|       2 | 1332 | `}` |
|    8445 | 1333 | `static sxi32 VmInstallDirectory(ph7_vm *pVm)` |
|       5 | 1334 | `{` |
|       - | 1335 | `	static const PH7_NativePropDef aDirProp[] = {` |
|       - | 1336 | `		{ DIR_PATH,   PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1337 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|       - | 1338 | `		{ DIR_HANDLE, PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1339 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "mixed" },` |
|       - | 1340 | `	};` |
|       - | 1341 | `	static const PH7_NativeMethodDef aDirMethod[] = {` |
|       - | 1342 | `		{ "close",  PH7_MOD_PUBLIC, "", "void", vm_builtin_Directory_close },` |
|       - | 1343 | `		{ "rewind", PH7_MOD_PUBLIC, "", "void", vm_builtin_Directory_rewind },` |
|       - | 1344 | `		{ "read",   PH7_MOD_PUBLIC, "", "string\|false", vm_builtin_Directory_read },` |
|       - | 1345 | `	};` |
|       - | 1346 | `	static const PH7_NativeClassSpec sSpec = {` |
|       - | 1347 | `		"Directory", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE,` |
|       - | 1348 | `		aDirMethod, SX_ARRAYSIZE(aDirMethod), 0, 0,` |
|       - | 1349 | `		aDirProp, SX_ARRAYSIZE(aDirProp), 0, 0, 0` |
|       - | 1350 | `	};` |
|    8450 | 1351 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|    8450 | 1352 | `	if( rc == SXRET_OK ){` |
|    8450 | 1353 | `		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"Directory",sizeof("Directory")-1,FALSE,0);` |
|    8450 | 1354 | `		if( pClass ){` |
|       - | 1355 | `			/* php words this refusal per class rather than with the generic` |
|       - | 1356 | `			 * "Instantiation of class %s is not allowed". */` |
|    8450 | 1357 | `			pClass->zNewRefusal = "Cannot directly construct Directory, use dir() instead";` |
|    4217 | 1358 | `		}` |
|    4217 | 1359 | `	}` |
|    8450 | 1360 | `	return rc;` |
|       5 | 1361 | `}` |
|       - | 1362 | `/*` |
|       - | 1363 | ` * ---------------------------------------------------------------------------` |
|       - | 1364 | ` * php's attribute classes.` |
|       - | 1365 | ` *` |
|       - | 1366 | ``  * Each carries an ATTRIBUTE of its own — `#[Attribute(Attribute::TARGET_CLASS)]` `` |
|       - | 1367 | ` * on Attribute, a target mask on every other one — and those records are` |
|       - | 1368 | ` * load-bearing rather than decorative: the engine reads them to decide whether a` |
|       - | 1369 | `` * user's `#[Deprecated]` may sit where it does, and ReflectionAttribute answers`` |
|       - | 1370 | ` * them. A compiled attribute holds its argument as byte-code, so this is what` |
|       - | 1371 | `` * `PH7_NativeClassAddAttribute()` exists for (rule 11's next unused corner,`` |
|       - | 1372 | ` * exercised here): the argument rides as a literal.` |
|       - | 1373 | ` *` |
|       - | 1374 | ` * php's Deprecated mask is 87 — TARGET_CLASS\|FUNCTION\|METHOD\|CLASS_CONSTANT\|` |
|       - | 1375 | ` * CONSTANT — where the chunk wrote 86 and left the CLASS bit out.` |
|       - | 1376 | ` *` |
|       - | 1377 | ` * Three of them declare NOTHING but their own mask, because what they mean is a` |
|       - | 1378 | `` * question something else asks: `#[AllowDynamicProperties]` is read by the`` |
|       - | 1379 | `` * dynamic-property decision at the write site, `#[SensitiveParameter]` by the`` |
|       - | 1380 | `` * backtrace builder, `#[ReturnTypeWillChange]` by php's tentative-return-type`` |
|       - | 1381 | ` * check (which the scope policy non-deprecated policy removed, so nothing consults it` |
|       - | 1382 | ` * here). They still have to EXIST: a program that spells one and then asks` |
|       - | 1383 | `` * `getAttributes()[0]->newInstance()` gets php's object, not`` |
|       - | 1384 | `` * `Attribute class "AllowDynamicProperties" not found`.`` |
|       - | 1385 | ` *` |
|       - | 1386 | `` * `SensitiveParameterValue` is not an attribute at all — it is the box php puts`` |
|       - | 1387 | ` * a redacted argument in — but it belongs to the same feature and to the same` |
|       - | 1388 | ` * declaration site.` |
|       - | 1389 | ` * ---------------------------------------------------------------------------` |
|       - | 1390 | ` */` |
|       - | 1391 | ``/* php declares `public function __construct()` on the three marker attributes, so`` |
|       - | 1392 | `` * Reflection reports one and `new AllowDynamicProperties(1)` is an`` |
|       - | 1393 | ` * ArgumentCountError. The body has nothing to do: the object carries no state. */` |
|      10 | 1394 | `static int vm_builtin_AttrMarker_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1395 | `{` |
|       5 | 1396 | `	SXUNUSED(pCtx);` |
|       5 | 1397 | `	SXUNUSED(nArg);` |
|       5 | 1398 | `	SXUNUSED(apArg);` |
|      12 | 1399 | `	return PH7_OK;` |
|       2 | 1400 | `}` |
|       - | 1401 | `/* SensitiveParameterValue::__construct(mixed $value) / getValue() / __debugInfo() */` |
|       - | 1402 | `#define SPV_SLOT "value"` |
|      20 | 1403 | `static int vm_builtin_SensitiveParameterValue_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1404 | `{` |
|      21 | 1405 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      21 | 1406 | `	if( pThis && nArg > 0 ){` |
|      21 | 1407 | `		PH7_NativeSetProp(pCtx->pVm,pThis,SPV_SLOT,sizeof(SPV_SLOT)-1,apArg[0]);` |
|      10 | 1408 | `	}` |
|      21 | 1409 | `	return PH7_OK;` |
|       1 | 1410 | `}` |
|       6 | 1411 | `static int vm_builtin_SensitiveParameterValue_getValue(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1412 | `{` |
|       7 | 1413 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       7 | 1414 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,SPV_SLOT) : 0;` |
|       3 | 1415 | `	SXUNUSED(nArg);` |
|       3 | 1416 | `	SXUNUSED(apArg);` |
|       7 | 1417 | `	if( pVal ){` |
|       7 | 1418 | `		ph7_result_value(pCtx,pVal);` |
|       4 | 1419 | `	}else{` |
|     ! 0 | 1420 | `		ph7_result_null(pCtx);` |
|       - | 1421 | `	}` |
|       7 | 1422 | `	return PH7_OK;` |
|       1 | 1423 | `}` |
|       2 | 1424 | `static int vm_builtin_SensitiveParameterValue_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1425 | `{` |
|       3 | 1426 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|       1 | 1427 | `	SXUNUSED(nArg);` |
|       1 | 1428 | `	SXUNUSED(apArg);` |
|       3 | 1429 | `	if( pOut == 0 ){` |
|     ! 0 | 1430 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1431 | `	}` |
|       3 | 1432 | `	ph7_result_value(pCtx,pOut);` |
|       3 | 1433 | `	return PH7_OK;` |
|       2 | 1434 | `}` |
|       - | 1435 | `/*` |
|       - | 1436 | `` * php gives the class a `get_properties_for` handler that answers NULL for every`` |
|       - | 1437 | `` * purpose, so the box shows nothing to var_export, the `(array)` cast or`` |
|       - | 1438 | `` * json_encode either — not just to var_dump's `__debugInfo()`. The point of the`` |
|       - | 1439 | ` * class is that the value it holds does not leak onto a display surface.` |
|       - | 1440 | ` */` |
|       6 | 1441 | `static sxi32 VmPresentSensitiveParameterValue(ph7_vm *pVm,ph7_class_instance *pThis,` |
|       - | 1442 | `	ph7_value *pOut,int bDebug)` |
|       1 | 1443 | `{` |
|       3 | 1444 | `	SXUNUSED(pVm);` |
|       3 | 1445 | `	SXUNUSED(pThis);` |
|       3 | 1446 | `	SXUNUSED(pOut);` |
|       3 | 1447 | `	SXUNUSED(bDebug);` |
|       7 | 1448 | `	return SXRET_OK;   /* the empty shape, both handlers */` |
|       1 | 1449 | `}` |
|       8 | 1450 | `static int vm_builtin_Attribute_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1451 | `{` |
|       9 | 1452 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       9 | 1453 | `	if( pThis ){` |
|      16 | 1454 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,"flags",` |
|       7 | 1455 | `			nArg > 0 ? ph7_value_to_int64(apArg[0]) : 127);` |
|       4 | 1456 | `	}` |
|       9 | 1457 | `	return PH7_OK;` |
|       1 | 1458 | `}` |
|       - | 1459 | `/* NoDiscard::__construct(?string $message = null) */` |
|       2 | 1460 | `static int vm_builtin_NoDiscard_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1461 | `{` |
|       3 | 1462 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - | 1463 | `	ph7_value sVal;` |
|       3 | 1464 | `	if( pThis == 0 ){` |
|     ! 0 | 1465 | `		return PH7_OK;` |
|       - | 1466 | `	}` |
|       3 | 1467 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|       3 | 1468 | `	if( nArg > 0 ){` |
|     ! 0 | 1469 | `		PH7_MemObjStore(apArg[0],&sVal);` |
|     ! 0 | 1470 | `	}` |
|       3 | 1471 | `	PH7_NativeSetProp(pCtx->pVm,pThis,"message",sizeof("message")-1,&sVal);` |
|       3 | 1472 | `	PH7_MemObjRelease(&sVal);` |
|       3 | 1473 | `	return PH7_OK;` |
|       2 | 1474 | `}` |
|      10 | 1475 | `static int vm_builtin_Deprecated_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1476 | `{` |
|      11 | 1477 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - | 1478 | `	static const char *const azSlot[] = { "message", "since" };` |
|       - | 1479 | `	int n;` |
|      11 | 1480 | `	if( pThis == 0 ){` |
|     ! 0 | 1481 | `		return PH7_OK;` |
|       - | 1482 | `	}` |
|      31 | 1483 | `	for( n = 0 ; n < 2 ; n++ ){` |
|       - | 1484 | `		ph7_value sVal;` |
|      21 | 1485 | `		PH7_MemObjInit(pCtx->pVm,&sVal);` |
|      21 | 1486 | `		if( n < nArg ){` |
|      15 | 1487 | `			PH7_MemObjStore(apArg[n],&sVal);` |
|       7 | 1488 | `		}` |
|      21 | 1489 | `		PH7_NativeSetProp(pCtx->pVm,pThis,azSlot[n],SyStrlen(azSlot[n]),&sVal);` |
|      21 | 1490 | `		PH7_MemObjRelease(&sVal);` |
|      11 | 1491 | `	}` |
|      11 | 1492 | `	return PH7_OK;` |
|       6 | 1493 | `}` |
|    8445 | 1494 | `static sxi32 VmInstallAttributes(ph7_vm *pVm)` |
|       5 | 1495 | `{` |
|       - | 1496 | `	static const PH7_NativeConstDef aAttrConst[] = {` |
|       - | 1497 | `		{ "TARGET_CLASS",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },` |
|       - | 1498 | `		{ "TARGET_FUNCTION",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|       - | 1499 | `		{ "TARGET_METHOD",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4, 0, 0.0 },` |
|       - | 1500 | `		{ "TARGET_PROPERTY",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 8, 0, 0.0 },` |
|       - | 1501 | `		{ "TARGET_CLASS_CONSTANT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16, 0, 0.0 },` |
|       - | 1502 | `		{ "TARGET_PARAMETER",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },` |
|       - | 1503 | `		{ "TARGET_CONSTANT",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64, 0, 0.0 },` |
|       - | 1504 | `		{ "TARGET_ALL",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 127, 0, 0.0 },` |
|       - | 1505 | `		{ "IS_REPEATABLE",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 128, 0, 0.0 },` |
|       - | 1506 | `	};` |
|       - | 1507 | ``	/* php declares `public int $flags;` — typed, NO default (the constructor is`` |
|       - | 1508 | ``	 * the only writer), which is what the chunk's `public $flags;` could not say. */`` |
|       - | 1509 | `	static const PH7_NativePropDef aAttrProp[] = {` |
|       - | 1510 | `		{ "flags", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|       - | 1511 | `	};` |
|       - | 1512 | `	static const PH7_NativeMethodDef aAttrMethod[] = {` |
|       - | 1513 | `		{ "__construct", PH7_MOD_PUBLIC, "int $flags = Attribute::TARGET_ALL", 0,` |
|       - | 1514 | `		  vm_builtin_Attribute_construct },` |
|       - | 1515 | `	};` |
|       - | 1516 | `	static const PH7_NativePropDef aDepProp[] = {` |
|       - | 1517 | `		{ "message", PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1518 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "?string" },` |
|       - | 1519 | `		{ "since",   PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1520 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "?string" },` |
|       - | 1521 | `	};` |
|       - | 1522 | `	static const PH7_NativeMethodDef aDepMethod[] = {` |
|       - | 1523 | `		{ "__construct", PH7_MOD_PUBLIC, "?string $message = null, ?string $since = null", 0,` |
|       - | 1524 | `		  vm_builtin_Deprecated_construct },` |
|       - | 1525 | `	};` |
|       - | 1526 | ``	/* NoDiscard is Deprecated's shape minus the `since`. */`` |
|       - | 1527 | `	static const PH7_NativePropDef aNdProp[] = {` |
|       - | 1528 | `		{ "message", PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1529 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "?string" },` |
|       - | 1530 | `	};` |
|       - | 1531 | `	static const PH7_NativeMethodDef aNdMethod[] = {` |
|       - | 1532 | `		{ "__construct", PH7_MOD_PUBLIC, "?string $message = null", 0,` |
|       - | 1533 | `		  vm_builtin_NoDiscard_construct },` |
|       - | 1534 | `	};` |
|       - | 1535 | `	/* The three markers: one argless constructor each and no state at all. */` |
|       - | 1536 | `	static const PH7_NativeMethodDef aMarkerMethod[] = {` |
|       - | 1537 | `		{ "__construct", PH7_MOD_PUBLIC, "", 0, vm_builtin_AttrMarker_construct },` |
|       - | 1538 | `	};` |
|       - | 1539 | `	static const PH7_NativePropDef aSpvProp[] = {` |
|       - | 1540 | `		{ SPV_SLOT, PH7_MOD_PRIVATE\|PH7_MOD_READONLY,` |
|       - | 1541 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "mixed" },` |
|       - | 1542 | `	};` |
|       - | 1543 | `	static const PH7_NativeMethodDef aSpvMethod[] = {` |
|       - | 1544 | `		{ "__construct", PH7_MOD_PUBLIC, "mixed $value", 0,` |
|       - | 1545 | `		  vm_builtin_SensitiveParameterValue_construct },` |
|       - | 1546 | `		{ "getValue",    PH7_MOD_PUBLIC, "", "mixed",` |
|       - | 1547 | `		  vm_builtin_SensitiveParameterValue_getValue },` |
|       - | 1548 | `		{ "__debugInfo", PH7_MOD_PUBLIC, "", "array",` |
|       - | 1549 | `		  vm_builtin_SensitiveParameterValue_debugInfo },` |
|       - | 1550 | `	};` |
|       - | 1551 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1552 | `		{ "Attribute", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1553 | `		  aAttrMethod, SX_ARRAYSIZE(aAttrMethod), aAttrConst, SX_ARRAYSIZE(aAttrConst),` |
|       - | 1554 | `		  aAttrProp, SX_ARRAYSIZE(aAttrProp), 0, 0, 0 },` |
|       - | 1555 | `		{ "Deprecated", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1556 | `		  aDepMethod, SX_ARRAYSIZE(aDepMethod), 0, 0,` |
|       - | 1557 | `		  aDepProp, SX_ARRAYSIZE(aDepProp), 0, 0, 0 },` |
|       - | 1558 | `		{ "AllowDynamicProperties", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1559 | `		  aMarkerMethod, SX_ARRAYSIZE(aMarkerMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1560 | `		{ "SensitiveParameter", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1561 | `		  aMarkerMethod, SX_ARRAYSIZE(aMarkerMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1562 | `		{ "ReturnTypeWillChange", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1563 | `		  aMarkerMethod, SX_ARRAYSIZE(aMarkerMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1564 | `		{ "Override", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1565 | `		  aMarkerMethod, SX_ARRAYSIZE(aMarkerMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1566 | `		{ "NoDiscard", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1567 | `		  aNdMethod, SX_ARRAYSIZE(aNdMethod), 0, 0,` |
|       - | 1568 | `		  aNdProp, SX_ARRAYSIZE(aNdProp), 0, 0, 0 },` |
|       - | 1569 | `		/* php 8.5's marker for an attribute whose TARGET is checked late. It is` |
|       - | 1570 | `		 * the one attribute class php declares with no constructor at all --` |
|       - | 1571 | `		 * every other marker here has the empty one -- so a script writes it` |
|       - | 1572 | ``		 * bare and `newInstance()` builds it with nothing. */`` |
|       - | 1573 | `		{ "DelayedTargetValidation", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1574 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1575 | `		/* php refuses BOTH directions for the box (ZEND_ACC_NOT_SERIALIZABLE), which` |
|       - | 1576 | `		 * is the whole point: a redacted value must not reach a payload either. */` |
|       - | 1577 | `		{ "SensitiveParameterValue", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOSERIALIZE,` |
|       - | 1578 | `		  aSpvMethod, SX_ARRAYSIZE(aSpvMethod), 0, 0,` |
|       - | 1579 | `		  aSpvProp, SX_ARRAYSIZE(aSpvProp), 0, 0,` |
|       - | 1580 | `		  VmPresentSensitiveParameterValue },` |
|       - | 1581 | `	};` |
|       - | 1582 | ``	/* Each attribute class's own `#[Attribute(mask)]`, php's masks verbatim. The`` |
|       - | 1583 | `	 * literal rows are STATIC because PH7_NativeClassAddAttribute keeps a pointer` |
|       - | 1584 | `	 * to them for the VM's lifetime. */` |
|       - | 1585 | `	static const PH7_NativeAttrArg aMaskClass[]  = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 1,  0, 0.0 } } };` |
|       - | 1586 | `	static const PH7_NativeAttrArg aMaskDep[]    = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 87, 0, 0.0 } } };` |
|       - | 1587 | `	static const PH7_NativeAttrArg aMaskParam[]  = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 32, 0, 0.0 } } };` |
|       - | 1588 | `	static const PH7_NativeAttrArg aMaskMethod[] = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 4,  0, 0.0 } } };` |
|       - | 1589 | `	static const PH7_NativeAttrArg aMaskMembr[]  = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 12, 0, 0.0 } } };` |
|       - | 1590 | `	static const PH7_NativeAttrArg aMaskCallee[] = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 6,  0, 0.0 } } };` |
|       - | 1591 | `	static const PH7_NativeAttrArg aMaskAll[]    = { { 0, { 0, 0, PH7_NATIVE_VAL_INT, 127,0, 0.0 } } };` |
|       - | 1592 | `	static const struct {` |
|       - | 1593 | `		const char *zClass;` |
|       - | 1594 | `		const PH7_NativeAttrArg *aArg;   /* php's TARGET_* mask for that class */` |
|       - | 1595 | `	} aOwnAttr[] = {` |
|       - | 1596 | `		{ "Attribute",              aMaskClass  },   /* TARGET_CLASS */` |
|       - | 1597 | `		{ "Deprecated",             aMaskDep    },   /* CLASS\|FUNCTION\|METHOD\|CLASS_CONSTANT\|CONSTANT */` |
|       - | 1598 | `		{ "AllowDynamicProperties", aMaskClass  },   /* TARGET_CLASS */` |
|       - | 1599 | `		{ "SensitiveParameter",     aMaskParam  },   /* TARGET_PARAMETER */` |
|       - | 1600 | `		{ "ReturnTypeWillChange",   aMaskMethod },   /* TARGET_METHOD */` |
|       - | 1601 | `		{ "Override",               aMaskMembr  },   /* METHOD\|PROPERTY (php 8.5) */` |
|       - | 1602 | `		{ "NoDiscard",              aMaskCallee },   /* FUNCTION\|METHOD (php 8.5) */` |
|       - | 1603 | `		{ "DelayedTargetValidation", aMaskAll   },   /* TARGET_ALL (php 8.5) */` |
|       - | 1604 | `	};` |
|    8450 | 1605 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       - | 1606 | `	sxu32 n;` |
|   76010 | 1607 | `	for( n = 0 ; rc == SXRET_OK && n < SX_ARRAYSIZE(aOwnAttr) ; ++n ){` |
|  101301 | 1608 | `		rc = PH7_NativeClassAddAttribute(&(*pVm),` |
|  101296 | 1609 | `			PH7_VmExtractClass(&(*pVm),aOwnAttr[n].zClass,` |
|   67560 | 1610 | `				(sxu32)SyStrlen(aOwnAttr[n].zClass),FALSE,0),` |
|   67560 | 1611 | `			"Attribute",aOwnAttr[n].aArg,1);` |
|   33741 | 1612 | `	}` |
|    8450 | 1613 | `	return rc;` |
|       5 | 1614 | `}` |
|       - | 1615 | `/*` |
|       - | 1616 | ` * stdClass and Random\RandomException.` |
|       - | 1617 | ` *` |
|       - | 1618 | ` * stdClass is EMPTY in php too — it holds only dynamic properties — so the whole` |
|       - | 1619 | `` * declaration is the row. `Random\RandomException` is the first NAMESPACED class`` |
|       - | 1620 | ` * declared from C: the engine keys its class table by the FULLY QUALIFIED name` |
|       - | 1621 | `` * (the compiler resolves `namespace Random { class RandomException }` to exactly`` |
|       - | 1622 | ` * this string before installing), so a spec row spells the FQN and needs no` |
|       - | 1623 | ` * namespace machinery at all. It also retires the chunk this file kept ALONE for` |
|       - | 1624 | `` * it, whose comment explains why: a `namespace` declaration is not reset at its`` |
|       - | 1625 | ` * closing brace here, so anything following it in the same chunk would have` |
|       - | 1626 | ` * leaked into the Random namespace.` |
|       - | 1627 | ` */` |
|    8445 | 1628 | `static sxi32 VmInstallStdClasses(ph7_vm *pVm)` |
|       5 | 1629 | `{` |
|       - | 1630 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1631 | `		{ "stdClass", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1632 | `		/* unserialize()'s carrier for a disallowed or unknown class: as empty as` |
|       - | 1633 | `		 * stdClass (its properties are the payload's, created dynamically); what` |
|       - | 1634 | `		 * makes it special is the pVm->pIncClass checks at the access sites. */` |
|       - | 1635 | `		{ "__PHP_Incomplete_Class", 0, 0, PH7_CLASS_FINAL, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1636 | `		{ "Random\\RandomException", "Exception", 0, PH7_CLASS_NOCLONE,` |
|       - | 1637 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1638 | `		/* php 8.5's filter exceptions: FILTER_THROW_ON_FAILURE raises the second,` |
|       - | 1639 | `		 * and the first is the base a caller catches to mean "any filter error". */` |
|       - | 1640 | `		{ "Filter\\FilterException", "Exception", 0, 0,` |
|       - | 1641 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1642 | `		{ "Filter\\FilterFailedException", "Filter\\FilterException", 0, 0,` |
|       - | 1643 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1644 | `	};` |
|    8450 | 1645 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       5 | 1646 | `}` |
|    8445 | 1647 | `PH7_PRIVATE sxi32 PH7_VmInstallBuiltinLib(ph7_vm *pVm)` |
|       5 | 1648 | `{` |
|       - | 1649 | `	SyString sBuiltin;` |
|       - | 1650 | `	/* The interfaces first: everything below implements one of them` |
|       - | 1651 | `	 * (Exception implements Throwable). */` |
|    8450 | 1652 | `	VmInstallCoreInterfaces(&(*pVm));` |
|    8450 | 1653 | `	VmInstallExceptions(&(*pVm));` |
|    8450 | 1654 | `	VmInstallStdClasses(&(*pVm));` |
|    8450 | 1655 | `	VmInstallDirectory(&(*pVm));` |
|    8450 | 1656 | `	VmInstallAttributes(&(*pVm));` |
|    8450 | 1657 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|       - | 1658 | `	/* Compile the built-in library */` |
|    8450 | 1659 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|    8450 | 1660 | `	return SXRET_OK;` |
|       5 | 1661 | `}` |
|       - | 1662 |  |
