# src/ph7/vm_builtin_lib.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 452/519 lines (87.09%)

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
|       - |   33 | `	"function scandir(string $directory,int $sorting_order = SCANDIR_SORT_ASCENDING, $context = null)"\` |
|       - |   34 | `    "{"\` |
|       - |   35 | `	"  /* php's Z_PARAM_PATH refusal, spelled here because this builtin is prelude PHP:"\` |
|       - |   36 | `	"     the C screen (VmBuiltinPathMask) reaches host builtins only, so without this"\` |
|       - |   37 | `	"     the NUL reached opendir() and the ValueError named opendir(), not scandir(). */"\` |
|       - |   38 | `	"  if( strpos($directory, chr(0)) !== false ){"\` |
|       - |   39 | `	"    throw new ValueError('scandir(): Argument #1 ($directory) must not contain any null bytes');"\` |
|       - |   40 | `	"  }"\` |
|       - |   41 | ``	"  /* php's `?resource $context` refusal, spelled here for the same reason the"\`` |
|       - |   42 | `	"     NUL check above is: forwarding an invalid one to opendir() would name"\` |
|       - |   43 | `	"     opendir() in a message php raises against scandir(). */"\` |
|       - |   44 | `	"  if( $context !== null ){"\` |
|       - |   45 | `	"    if( !is_resource($context) ){"\` |
|       - |   46 | `	"      throw new TypeError('scandir(): Argument #3 ($context) must be of type resource or null, '"\` |
|       - |   47 | `	"        . get_debug_type($context) . ' given');"\` |
|       - |   48 | `	"    }"\` |
|       - |   49 | `	"    if( get_resource_type($context) !== 'stream-context' ){"\` |
|       - |   50 | `	"      throw new TypeError('scandir(): supplied resource is not a valid Stream-Context resource');"\` |
|       - |   51 | `	"    }"\` |
|       - |   52 | `	"  }"\` |
|       - |   53 | `	"  $aDir = array();"\` |
|       - |   54 | `	"  /* php hands the context straight to the open it performs; dropping it here"\` |
|       - |   55 | `	"     was the same unread argument every C opener had. */"\` |
|       - |   56 | `	"  $pHandle = opendir($directory, $context);"\` |
|       - |   57 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|       - |   58 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|       - |   59 | `	"      $aDir[] = $pEntry;"\` |
|       - |   60 | `	"   }"\` |
|       - |   61 | `	"  closedir($pHandle);"\` |
|       - |   62 | `	"  /* php's rule is a two-way split, not a three-value enum: SORT_NONE leaves the"\` |
|       - |   63 | `	"     order alone and EVERY other value sorts -- ascending only for the exact"\` |
|       - |   64 | `	"     SORT_ASCENDING, descending otherwise. PHL left an unknown value UNSORTED,"\` |
|       - |   65 | `	"     which reads as SORT_NONE."\` |
|       - |   66 | `	"     The comparison is php_stream_dirent_alphasort's strcoll(), which in the C"\` |
|       - |   67 | `	"     locale php runs in is a BYTE compare -- so SORT_STRING, not the default"\` |
|       - |   68 | `	"     SORT_REGULAR. Sorting by VALUE ordered numeric names numerically, which is"\` |
|       - |   69 | ``	"     a different listing: `9` came before `10`, and `00` before `0`. */"\`` |
|       - |   70 | `	"  if( $sorting_order != SCANDIR_SORT_NONE ){"\` |
|       - |   71 | `	"      if( $sorting_order == SCANDIR_SORT_ASCENDING ){ sort($aDir,SORT_STRING); }"\` |
|       - |   72 | `	"      else { rsort($aDir,SORT_STRING); }"\` |
|       - |   73 | `	"  }"\` |
|       - |   74 | `	"  return $aDir;"\` |
|       - |   75 | `	"}"\` |
|       - |   76 | `	"function glob(string $pattern,int $flags = 0){"\` |
|       - |   77 | `	"/* php's Z_PARAM_PATH refusal (see scandir above). It precedes the flag check:"\` |
|       - |   78 | `	"   php's ZPP runs before the function body. Without it the NUL was simply the end"\` |
|       - |   79 | `	"   of the pattern and glob() answered for the truncated one. */"\` |
|       - |   80 | `	"if( strpos($pattern, chr(0)) !== false ){"\` |
|       - |   81 | `	"  throw new ValueError('glob(): Argument #1 ($pattern) must not contain any null bytes');"\` |
|       - |   82 | `	"}"\` |
|       - |   83 | `	"/* php rejects a mask holding any bit outside GLOB_AVAILABLE_FLAGS with a warning"\` |
|       - |   84 | `	"   and FALSE. PHL accepted anything and just tested the bits it knew, so a stale"\` |
|       - |   85 | `	"   script passing the OLD PHL glob values (1/2/4/...) silently got a plain glob. */"\` |
|       - |   86 | `	"if( $flags & ~GLOB_AVAILABLE_FLAGS ){"\` |
|       - |   87 | `	"  trigger_error('glob(): At least one of the passed flags is invalid or not supported on this platform', E_USER_WARNING);"\` |
|       - |   88 | `	"  return FALSE;"\` |
|       - |   89 | `	"}"\` |
|       - |   90 | `	"/* GLOB_BRACE: expand the FIRST top-level {a,b,...} group and glob each"\` |
|       - |   91 | `	"   alternative IN ORDER, concatenating the answers (each sub-glob sorts its"\` |
|       - |   92 | `	"   own results; php never re-sorts across alternatives). Nested groups are"\` |
|       - |   93 | `	"   handled by the recursion, and GLOB_NOCHECK applies per EXPANDED pattern,"\` |
|       - |   94 | `	"   which is php's answer too. The flag used to be accepted and IGNORED, so"\` |
|       - |   95 | `	"   any braced pattern answered [] in silence. */"\` |
|       - |   96 | `	"if( $flags & GLOB_BRACE ){"\` |
|       - |   97 | `	"  $nLen = strlen($pattern); $iOpen = -1; $iClose = -1; $iDepth = 0;"\` |
|       - |   98 | `	"  for( $i = 0 ; $i < $nLen ; $i++ ){"\` |
|       - |   99 | `	"    $ch = $pattern[$i];"\` |
|       - |  100 | `	"    if( $ch === '{' ){ if( $iDepth === 0 ){ $iOpen = $i; } $iDepth++; }"\` |
|       - |  101 | `	"    else if( $ch === '}' && $iDepth > 0 ){ $iDepth--; if( $iDepth === 0 ){ $iClose = $i; break; } }"\` |
|       - |  102 | `	"  }"\` |
|       - |  103 | `	"  if( $iOpen >= 0 && $iClose > $iOpen ){"\` |
|       - |  104 | `	"    $zHead = substr($pattern,0,$iOpen);"\` |
|       - |  105 | `	"    $zBody = (string)substr($pattern,$iOpen+1,$iClose-$iOpen-1);"\` |
|       - |  106 | `	"    $zTail = (string)substr($pattern,$iClose+1);"\` |
|       - |  107 | `	"    $aAlt = array(); $zCur = ''; $iDepth = 0;"\` |
|       - |  108 | `	"    for( $i = 0 ; $i < strlen($zBody) ; $i++ ){"\` |
|       - |  109 | `	"      $ch = $zBody[$i];"\` |
|       - |  110 | `	"      if( $ch === '{' ){ $iDepth++; }"\` |
|       - |  111 | `	"      else if( $ch === '}' ){ $iDepth--; }"\` |
|       - |  112 | `	"      if( $ch === ',' && $iDepth === 0 ){ $aAlt[] = $zCur; $zCur = ''; continue; }"\` |
|       - |  113 | `	"      $zCur .= $ch;"\` |
|       - |  114 | `	"    }"\` |
|       - |  115 | `	"    $aAlt[] = $zCur;"\` |
|       - |  116 | `	"    $pArray = array();"\` |
|       - |  117 | `	"    foreach( $aAlt as $zAlt ){"\` |
|       - |  118 | `	"      $aSub = glob($zHead . $zAlt . $zTail,$flags);"\` |
|       - |  119 | `	"      if( $aSub !== false ){ foreach( $aSub as $zHit ){ $pArray[] = $zHit; } }"\` |
|       - |  120 | `	"    }"\` |
|       - |  121 | `	"    return $pArray;"\` |
|       - |  122 | `	"  }"\` |
|       - |  123 | `	"}"\` |
|       - |  124 | `	"/* A pattern that ENDS in a slash names DIRECTORIES, and php keeps the slash:"\` |
|       - |  125 | `	"   glob('d/') is ['d/'] and glob('d/*' . '/') is ['d/a/','d/b/']. Answer the base"\` |
|       - |  126 | `	"   without it, as directories, and put it back -- ONE slash at a time, so a"\` |
|       - |  127 | `	"   pattern ending in two keeps both. */"\` |
|       - |  128 | `	"if( substr($pattern,-1) === '/' ){"\` |
|       - |  129 | `	"  $zBase = substr($pattern,0,-1);"\` |
|       - |  130 | `	"  if( $zBase === '' ){"\` |
|       - |  131 | `	"    /* the pattern was '/' itself */"\` |
|       - |  132 | `	"    $pArray = is_dir('/') ? array('/') : array();"\` |
|       - |  133 | `	"  }else{"\` |
|       - |  134 | `	"    $pArray = array();"\` |
|       - |  135 | `	"    foreach( glob($zBase,($flags & ~(GLOB_MARK\|GLOB_NOCHECK)) \| GLOB_ONLYDIR) as $zHit ){"\` |
|       - |  136 | `	"      $pArray[] = $zHit . '/';"\` |
|       - |  137 | `	"    }"\` |
|       - |  138 | ``	"    /* php sorts the names it ANSWERS, slash included, so `a/../` comes before"\`` |
|       - |  139 | ``	"       `a/./` -- sorting the bases and appending afterwards has them the other"\`` |
|       - |  140 | `	"       way round. */"\` |
|       - |  141 | `	"    if( ($flags & GLOB_NOSORT) == 0 ){ sort($pArray,SORT_STRING); }"\` |
|       - |  142 | `	"  }"\` |
|       - |  143 | `	"  if( ($flags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){ $pArray[] = $pattern; }"\` |
|       - |  144 | `	"  return $pArray;"\` |
|       - |  145 | `	"}"\` |
|       - |  146 | `	"/* A wildcard in the DIRECTORY part is matched LEVEL BY LEVEL, which is what"\` |
|       - |  147 | `	"   glob(3) does: list the directories that part names, then glob the last"\` |
|       - |  148 | `	"   component inside each. Reading only the last component -- all this used to"\` |
|       - |  149 | ``	"   do -- answered [] for `src/*' . '/*.php', the everyday two-level spelling,"\`` |
|       - |  150 | `	"   and for every deeper one. */"\` |
|       - |  151 | `	"$slash = strrpos($pattern,'/');"\` |
|       - |  152 | `	"if( $slash !== false ){"\` |
|       - |  153 | `	"  $zHead = substr($pattern,0,$slash);"\` |
|       - |  154 | `	"  if( $zHead !== '' && strcspn($zHead,'*?[') != strlen($zHead) ){"\` |
|       - |  155 | `	"    $pArray = array();"\` |
|       - |  156 | `	"    foreach( glob($zHead . '/') as $zDirHit ){"\` |
|       - |  157 | `	"      foreach( glob($zDirHit . substr($pattern,$slash+1),$flags & ~GLOB_NOCHECK) as $zHit ){"\` |
|       - |  158 | `	"        $pArray[] = $zHit;"\` |
|       - |  159 | `	"      }"\` |
|       - |  160 | `	"    }"\` |
|       - |  161 | `	"    if( ($flags & GLOB_NOSORT) == 0 ){ sort($pArray,SORT_STRING); }"\` |
|       - |  162 | `	"    if( ($flags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){ $pArray[] = $pattern; }"\` |
|       - |  163 | `	"    return $pArray;"\` |
|       - |  164 | `	"  }"\` |
|       - |  165 | `	"}"\` |
|       - |  166 | `	"/* php keeps the literal directory portion of the pattern in every result;"\` |
|       - |  167 | `	"   split off everything up to and including the last '/' as the prefix. */"\` |
|       - |  168 | `	"$slash = strrpos($pattern,'/');"\` |
|       - |  169 | `	"if( $slash === false ){ $zDir = '.'; $prefix = ''; $pat = $pattern; }"\` |
|       - |  170 | `	"else { $zDir = substr($pattern,0,$slash); if( $zDir === '' ){ $zDir = '/'; } $prefix = substr($pattern,0,$slash+1); $pat = substr($pattern,$slash+1); }"\` |
|       - |  171 | `	"$pArray = array(); /* Empty array */"\` |
|       - |  172 | `	"/* php answers [] in SILENCE for a directory that cannot be opened — a"\` |
|       - |  173 | `	"   nonexistent path is simply zero matches (GLOB_ERR included; that flag is"\` |
|       - |  174 | `	"   about errors during the walk, not about the path). PHL used to let"\` |
|       - |  175 | `	"   opendir() warn and answered FALSE. */"\` |
|       - |  176 | `	"$pHandle = @opendir($zDir);"\` |
|       - |  177 | `	"if( $pHandle != FALSE ){"\` |
|       - |  178 | `	"/* Loop throw available entries */"\` |
|       - |  179 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|       - |  180 | `	" /* php's glob() never matches a leading-dot entry (incl. '.' and '..') unless"\` |
|       - |  181 | `	"    the pattern itself starts with a dot */"\` |
|       - |  182 | `	"	if( strlen($pEntry) > 0 && $pEntry[0] === '.' && (strlen($pat) < 1 \|\| $pat[0] !== '.') ){ continue; }"\` |
|       - |  183 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|       - |  184 | `	"	$rc = strglob($pat,$pEntry);"\` |
|       - |  185 | `	"	if( $rc ){"\` |
|       - |  186 | `	"	   $zFull = $prefix . $pEntry;"\` |
|       - |  187 | `	"	   if( is_dir($zDir . '/' . $pEntry) ){"\` |
|       - |  188 | `	"	      if( $flags & GLOB_MARK ){"\` |
|       - |  189 | `	"		     /* Adds a slash to each directory returned */"\` |
|       - |  190 | `	"			 $zFull .= DIRECTORY_SEPARATOR;"\` |
|       - |  191 | `	"		  }"\` |
|       - |  192 | `	"	   }else if( $flags & GLOB_ONLYDIR ){"\` |
|       - |  193 | `	"	     /* Not a directory,ignore */"\` |
|       - |  194 | `	"		 continue;"\` |
|       - |  195 | `	"	   }"\` |
|       - |  196 | `	"	   /* Add the entry (with its literal directory prefix, php-style) */"\` |
|       - |  197 | `	"	   $pArray[] = $zFull;"\` |
|       - |  198 | `	"	}"\` |
|       - |  199 | `	" }"\` |
|       - |  200 | `	"/* Close the handle */"\` |
|       - |  201 | `	"closedir($pHandle);"\` |
|       - |  202 | `	"}"\` |
|       - |  203 | `	"if( ($flags & GLOB_NOSORT) == 0 ){"\` |
|       - |  204 | `	"  /* glob(3) sorts with strcoll(), a BYTE compare in the C locale php runs in,"\` |
|       - |  205 | `	"     and it sorts the whole ANSWER rather than each directory it walked. The"\` |
|       - |  206 | `	"     default SORT_REGULAR compared numeric names as NUMBERS, so a directory of"\` |
|       - |  207 | ``	"     `1.jpg`..`10.jpg` came back in a different order than php lists it. */"\`` |
|       - |  208 | `	"  sort($pArray,SORT_STRING);"\` |
|       - |  209 | `	"}"\` |
|       - |  210 | `	"if( ($flags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|       - |  211 | `	"  /* Return the search pattern if no files matching were found */"\` |
|       - |  212 | `	"  $pArray[] = $pattern;"\` |
|       - |  213 | `	"}"\` |
|       - |  214 | `	"/* Return the created array */"\` |
|       - |  215 | `	"return $pArray;"\` |
|       - |  216 | `   "}"\` |
|       - |  217 | `   "/* Creates a temporary file */"\` |
|       - |  218 | `   "function tmpfile(){"\` |
|       - |  219 | `   "  /* Extract the temp directory */"\` |
|       - |  220 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|       - |  221 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|       - |  222 | `   "    /* Use the current dir */"\` |
|       - |  223 | `   "    $zTempDir = '.';"\` |
|       - |  224 | `   "  }"\` |
|       - |  225 | `   "  /* Create the file */"\` |
|       - |  226 | `   "  $zPath = $zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12);"\` |
|       - |  227 | `   "  /* php CREATES the file and then opens it r+b, which is the mode"\` |
|       - |  228 | `   "   * stream_get_meta_data() reports back for it. */"\` |
|       - |  229 | `   "  fclose(fopen($zPath,'w'));"\` |
|       - |  230 | `   "  $pHandle = fopen($zPath,'r+b');"\` |
|       - |  231 | `   "  return $pHandle;"\` |
|       - |  232 | `   "}"\` |
|       - |  233 | `   "function is_nan($num){ $num = (float)$num; return $num != $num; }"\` |
|       - |  234 | `   "function is_infinite($num){ $num = (float)$num; return $num == INF \|\| $num == -INF; }"\` |
|       - |  235 | `   "function is_finite($num){ $num = (float)$num; return !is_nan($num) && !is_infinite($num); }"\` |
|       - |  236 | `   "/* Inverse of bin2hex() */"\` |
|       - |  237 | `   "function hex2bin($string){"\` |
|       - |  238 | `   "  $string = (string)$string;"\` |
|       - |  239 | `   "  $len = strlen($string);"\` |
|       - |  240 | `   "  if( $len % 2 !== 0 ){"\` |
|       - |  241 | `   "    trigger_error('hex2bin(): Hexadecimal input string must have an even length', E_USER_WARNING);"\` |
|       - |  242 | `   "    return false;"\` |
|       - |  243 | `   "  }"\` |
|       - |  244 | `   "  $out = '';"\` |
|       - |  245 | `   "  for( $i = 0 ; $i < $len ; $i += 2 ){"\` |
|       - |  246 | `   "    $pair = substr($string, $i, 2);"\` |
|       - |  247 | `   "    if( !ctype_xdigit($pair) ){"\` |
|       - |  248 | `   "      trigger_error('hex2bin(): Input string must be hexadecimal string', E_USER_WARNING);"\` |
|       - |  249 | `   "      return false;"\` |
|       - |  250 | `   "    }"\` |
|       - |  251 | `   "    $out = $out . chr(hexdec($pair));"\` |
|       - |  252 | `   "  }"\` |
|       - |  253 | `   "  return $out;"\` |
|       - |  254 | `   "}"\` |
|       - |  255 | ``   "/* Division that never throws: INF/-INF/NAN like php. The two `float`"\`` |
|       - |  256 | `   " * declarations are php's own: they are what refuses a non-numeric string"\` |
|       - |  257 | `   " * (an untyped $num1 cast to 0.0 and DIVIDED, so fdiv('abc',2) answered"\` |
|       - |  258 | `   " * float(0)), and what ReflectionFunction prints. */"\` |
|       - |  259 | `   "function fdiv(float $num1, float $num2): float {"\` |
|       - |  260 | `   "  if( $num2 == 0.0 ){"\` |
|       - |  261 | `   "    if( $num1 == 0.0 \|\| is_nan($num1) ){ return NAN; }"\` |
|       - |  262 | `   "    return $num1 > 0 ? INF : -INF;"\` |
|       - |  263 | `   "  }"\` |
|       - |  264 | `   "  return $num1 / $num2;"\` |
|       - |  265 | `   "}"\` |
|       - |  266 | `   "function checkdate($month, $day, $year){"\` |
|       - |  267 | `   "  $month = (int)$month; $day = (int)$day; $year = (int)$year;"\` |
|       - |  268 | `   "  if( $month < 1 \|\| $month > 12 \|\| $year < 1 \|\| $year > 32767 \|\| $day < 1 ){ return false; }"\` |
|       - |  269 | `   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\` |
|       - |  270 | `   "  $max = $days[$month - 1];"\` |
|       - |  271 | `   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) \|\| ($year % 400 === 0)) ){"\` |
|       - |  272 | `   "    $max = 29;"\` |
|       - |  273 | `   "  }"\` |
|       - |  274 | `   "  return $day <= $max;"\` |
|       - |  275 | `   "}"\` |
|       - |  276 | `   "function is_iterable($value){ return is_array($value) \|\| ($value instanceof Traversable); }"\` |
|       - |  277 | `   "function is_countable($value){ return is_array($value) \|\| ($value instanceof Countable); }"\` |
|       - |  278 | `   "function doubleval($value){ return (float)$value; }"\` |
|       - |  279 | `   "function array_count_values($array){"\` |
|       - |  280 | `   "  $out = array();"\` |
|       - |  281 | `   "  foreach( $array as $v ){"\` |
|       - |  282 | `   "    if( !is_int($v) && !is_string($v) ){"\` |
|       - |  283 | `   "      trigger_error('array_count_values(): Can only count string and integer values, entry skipped', E_USER_WARNING);"\` |
|       - |  284 | `   "      continue;"\` |
|       - |  285 | `   "    }"\` |
|       - |  286 | `   "    if( isset($out[$v]) ){ $out[$v] = $out[$v] + 1; } else { $out[$v] = 1; }"\` |
|       - |  287 | `   "  }"\` |
|       - |  288 | `   "  return $out;"\` |
|       - |  289 | `   "}"\` |
|       - |  290 | `   "function array_change_key_case($array, $case = CASE_LOWER){"\` |
|       - |  291 | `   "  $out = array();"\` |
|       - |  292 | `   "  foreach( $array as $k => $v ){"\` |
|       - |  293 | `   "    if( is_string($k) ){ $k = ($case == CASE_UPPER) ? strtoupper($k) : strtolower($k); }"\` |
|       - |  294 | `   "    $out[$k] = $v;"\` |
|       - |  295 | `   "  }"\` |
|       - |  296 | `   "  return $out;"\` |
|       - |  297 | `   "}"\` |
|       - |  298 | `   "function array_replace_recursive($array, ...$replacements){"\` |
|       - |  299 | `   "  foreach( $replacements as $o ){"\` |
|       - |  300 | `   "    foreach( $o as $k => $v ){"\` |
|       - |  301 | `   "      if( is_array($v) && isset($array[$k]) && is_array($array[$k]) ){"\` |
|       - |  302 | `   "        $array[$k] = array_replace_recursive($array[$k], $v);"\` |
|       - |  303 | `   "      }else{"\` |
|       - |  304 | `   "        $array[$k] = $v;"\` |
|       - |  305 | `   "      }"\` |
|       - |  306 | `   "    }"\` |
|       - |  307 | `   "  }"\` |
|       - |  308 | `   "  return $array;"\` |
|       - |  309 | `   "}"\` |
|       - |  310 | `   "function class_uses($object_or_class, $autoload = true){"\` |
|       - |  311 | `   "  $c = is_object($object_or_class) ? get_class($object_or_class) : (string)$object_or_class;"\` |
|       - |  312 | `   "  if( !class_exists($c) ){ return false; }"\` |
|       - |  313 | `   "  return array();  /* PHL has no traits yet -- always the empty set */"\` |
|       - |  314 | `   "}"\` |
|       - |  315 | `   "function ip2long($ip){"\` |
|       - |  316 | `   "  $p = explode('.', (string)$ip);"\` |
|       - |  317 | `   "  if( count($p) !== 4 ){ return false; }"\` |
|       - |  318 | `   "  $n = 0;"\` |
|       - |  319 | `   "  foreach( $p as $o ){"\` |
|       - |  320 | `   "    if( !ctype_digit($o) \|\| (int)$o < 0 \|\| (int)$o > 255 ){ return false; }"\` |
|       - |  321 | `   "    $n = $n * 256 + (int)$o;"\` |
|       - |  322 | `   "  }"\` |
|       - |  323 | `   "  return $n;"\` |
|       - |  324 | `   "}"\` |
|       - |  325 | `   "function long2ip($ip){"\` |
|       - |  326 | `   "  $n = (int)$ip;"\` |
|       - |  327 | `   "  return (($n >> 24) & 255) . '.' . (($n >> 16) & 255) . '.' . (($n >> 8) & 255) . '.' . ($n & 255);"\` |
|       - |  328 | `   "}"\` |
|       - |  329 | `   "function preg_filter($pattern, $replacement, $subject, $limit = -1, &$count = null){"\` |
|       - |  330 | `   "  /* php declares &$count and always writes it -- the total number of"\` |
|       - |  331 | `   "   * replacements across every subject, 0 when nothing matched. PHL never"\` |
|       - |  332 | `   "   * declared the parameter, so a caller reading it got its previous value. */"\` |
|       - |  333 | `   "  if( is_array($subject) ){"\` |
|       - |  334 | `   "    $total = 0;"\` |
|       - |  335 | `   "    $out = array();"\` |
|       - |  336 | `   "    foreach( $subject as $k => $v ){"\` |
|       - |  337 | `   "      $r = preg_replace($pattern, $replacement, (string)$v, $limit, $cnt);"\` |
|       - |  338 | `   "      $total = $total + $cnt;"\` |
|       - |  339 | `   "      if( $cnt > 0 ){ $out[$k] = $r; }"\` |
|       - |  340 | `   "    }"\` |
|       - |  341 | `   "    $count = $total;"\` |
|       - |  342 | `   "    return $out;"\` |
|       - |  343 | `   "  }"\` |
|       - |  344 | `   "  $r = preg_replace($pattern, $replacement, (string)$subject, $limit, $cnt);"\` |
|       - |  345 | `   "  $count = $cnt;"\` |
|       - |  346 | `   "  return $cnt > 0 ? $r : null;"\` |
|       - |  347 | `   "}"\` |
|       - |  348 | `   "function preg_replace_callback_array($pattern, $subject, $limit = -1, &$count = null, $flags = 0){"\` |
|       - |  349 | `   "  /* &$count is the total across every pattern; $flags shapes each callback's"\` |
|       - |  350 | `   "   * match array. php writes &$count only when the whole run SUCCEEDED -- a"\` |
|       - |  351 | `   "   * pattern that fails to compile answers null and leaves it untouched (an"\` |
|       - |  352 | `   "   * array subject is not a failure: it degrades to the empty array, count 0). */"\` |
|       - |  353 | `   "  $total = 0;"\` |
|       - |  354 | `   "  foreach( $pattern as $pat => $cb ){"\` |
|       - |  355 | `   "    $subject = preg_replace_callback($pat, $cb, $subject, $limit, $cnt, $flags);"\` |
|       - |  356 | `   "    if( $subject === null ){ return null; }"\` |
|       - |  357 | `   "    $total = $total + $cnt;"\` |
|       - |  358 | `   "  }"\` |
|       - |  359 | `   "  $count = $total;"\` |
|       - |  360 | `   "  return $subject;"\` |
|       - |  361 | `   "}"\` |
|       - |  362 | `   "function preg_grep($pattern, $array, $flags = 0){"\` |
|       - |  363 | `   "  $out = array();"\` |
|       - |  364 | `   "  foreach( $array as $k => $v ){"\` |
|       - |  365 | `   "    $m = preg_match($pattern, (string)$v);"\` |
|       - |  366 | `   "    if( $flags & PREG_GREP_INVERT ){ $m = !$m; }"\` |
|       - |  367 | `   "    if( $m ){ $out[$k] = $v; }"\` |
|       - |  368 | `   "  }"\` |
|       - |  369 | `   "  return $out;"\` |
|       - |  370 | `   "}"\` |
|       - |  371 | `   "function class_implements($object_or_class, $autoload = true){"\` |
|       - |  372 | `   "  $c = is_object($object_or_class) ? get_class($object_or_class) : (string)$object_or_class;"\` |
|       - |  373 | `   "  if( !class_exists($c) && !interface_exists($c) ){ return false; }"\` |
|       - |  374 | `   "  $out = array();"\` |
|       - |  375 | `   "  $r = new ReflectionClass($c);"\` |
|       - |  376 | `   "  foreach( $r->getInterfaceNames() as $i ){ $out[$i] = $i; }"\` |
|       - |  377 | `   "  return $out;"\` |
|       - |  378 | `   "}"\` |
|       - |  379 | `   "function class_parents($object_or_class, $autoload = true){"\` |
|       - |  380 | `   "  $c = is_object($object_or_class) ? get_class($object_or_class) : (string)$object_or_class;"\` |
|       - |  381 | `   "  if( !class_exists($c) ){ return false; }"\` |
|       - |  382 | `   "  $out = array();"\` |
|       - |  383 | `   "  $r = new ReflectionClass($c);"\` |
|       - |  384 | `   "  while( ($p = $r->getParentClass()) ){"\` |
|       - |  385 | `   "    $n = $p->getName();"\` |
|       - |  386 | `   "    $out[$n] = $n;"\` |
|       - |  387 | `   "    $r = $p;"\` |
|       - |  388 | `   "  }"\` |
|       - |  389 | `   "  return $out;"\` |
|       - |  390 | `   "}"\` |
|       - |  391 | `   "/* php 8.3 str_increment(): Perl-style alphanumeric increment. */"\` |
|       - |  392 | `   "function str_increment($string){"\` |
|       - |  393 | `   "  $string = (string)$string;"\` |
|       - |  394 | `   "  if( $string === '' ){ throw new ValueError('str_increment(): Argument #1 ($string) must not be empty'); }"\` |
|       - |  395 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_increment(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|       - |  396 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|       - |  397 | `   "    $c = $string[$i];"\` |
|       - |  398 | `   "    if( $c === 'z' ){ $string[$i] = 'a'; }"\` |
|       - |  399 | `   "    elseif( $c === 'Z' ){ $string[$i] = 'A'; }"\` |
|       - |  400 | `   "    elseif( $c === '9' ){ $string[$i] = '0'; }"\` |
|       - |  401 | `   "    else { $string[$i] = chr(ord($c) + 1); return $string; }"\` |
|       - |  402 | `   "  }"\` |
|       - |  403 | `   "  $first = $string[0];"\` |
|       - |  404 | `   "  if( $first === '0' ){ return '1' . $string; }"\` |
|       - |  405 | `   "  if( $first === 'a' ){ return 'a' . $string; }"\` |
|       - |  406 | `   "  return 'A' . $string;"\` |
|       - |  407 | `   "}"\` |
|       - |  408 | `   "/* php 8.3 str_decrement(): inverse of str_increment(); throws out of range"\` |
|       - |  409 | `   " * at the bottom of the counting sequence. */"\` |
|       - |  410 | `   "function str_decrement($string){"\` |
|       - |  411 | `   "  $string = (string)$string;"\` |
|       - |  412 | `   "  if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) must not be empty'); }"\` |
|       - |  413 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_decrement(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|       - |  414 | `   "  $orig = $string;"\` |
|       - |  415 | `   "  $borrowed = false;"\` |
|       - |  416 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|       - |  417 | `   "    $c = $string[$i];"\` |
|       - |  418 | `   "    if( $c === 'a' ){ $string[$i] = 'z'; }"\` |
|       - |  419 | `   "    elseif( $c === 'A' ){ $string[$i] = 'Z'; }"\` |
|       - |  420 | `   "    elseif( $c === '0' ){ $string[$i] = '9'; }"\` |
|       - |  421 | `   "    else { $string[$i] = chr(ord($c) - 1); $borrowed = false; break; }"\` |
|       - |  422 | `   "    if( $i === 0 ){ $borrowed = true; }"\` |
|       - |  423 | `   "  }"\` |
|       - |  424 | `   "  if( $borrowed ){"\` |
|       - |  425 | `   "    if( $string[0] === '9' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|       - |  426 | `   "    $string = substr($string, 1);"\` |
|       - |  427 | `   "    if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|       - |  428 | `   "  } elseif( strlen($string) > 1 && $string[0] === '0' ){"\` |
|       - |  429 | `   "    $string = substr($string, 1);"\` |
|       - |  430 | `   "  }"\` |
|       - |  431 | `   "  return $string;"\` |
|       - |  432 | `   "}"\` |
|       - |  433 | `   /* fileperms/fileowner/filegroup/fileinode moved to C (vfs.c, VfsStatField):` |
|       - |  434 | `    * as prelude wrappers over stat() three of them said nothing on a failed stat` |
|       - |  435 | `    * and the fourth raised trigger_error, whose errno is E_USER_WARNING's 512 and` |
|       - |  436 | `    * whose line is this chunk's rather than the caller's. */\` |
|       - |  437 | `   "/* PH7 keeps no stat cache, so this is a no-op like php on a clean cache. */"\` |
|       - |  438 | `   "function clearstatcache($clear_realpath_cache = false, $filename = ''){}"\` |
|       - |  439 | `   /* mb_ucfirst/mb_lcfirst moved to C (builtin_mb.c): as prelude wrappers they` |
|       - |  440 | `    * dropped $encoding, UPPER-cased where php title-cases ('ß' -> 'SS' for php's` |
|       - |  441 | `    * 'Ss') and lowered a leading Σ with nothing after it, which is php's FINAL` |
|       - |  442 | `    * sigma and not what a first character gets. */\` |
|       - |  443 | `   "/* Creates a temporary file and returns its name */"\` |
|       - |  444 | `   "function tempnam(string $directory,string $prefix)"\` |
|       - |  445 | `   "{"\` |
|       - |  446 | `   "   /* php's Z_PARAM_PATH refusal on BOTH parameters (see scandir above); the prefix"\` |
|       - |  447 | `   "    * is a path fragment there too, and PHL used to build a filename with the NUL"\` |
|       - |  448 | `   "    * still in it. */"\` |
|       - |  449 | `   "   if( strpos($directory, chr(0)) !== false ){"\` |
|       - |  450 | `   "     throw new ValueError('tempnam(): Argument #1 ($directory) must not contain any null bytes');"\` |
|       - |  451 | `   "   }"\` |
|       - |  452 | `   "   if( strpos($prefix, chr(0)) !== false ){"\` |
|       - |  453 | `   "     throw new ValueError('tempnam(): Argument #2 ($prefix) must not contain any null bytes');"\` |
|       - |  454 | `   "   }"\` |
|       - |  455 | `   "   /* php CREATES the file (empty, mode 0600) and guarantees the name is unique --"\` |
|       - |  456 | `   "    * returning a bare name left the caller with a path that does not exist, so"\` |
|       - |  457 | `   "    * file_exists() was false and unlink() failed on it. */"\` |
|       - |  458 | `   "   $directory = rtrim($directory, DIRECTORY_SEPARATOR);"\` |
|       - |  459 | `   "   for( $i = 0 ; $i < 64 ; ++$i ){"\` |
|       - |  460 | `   "     $zPath = $directory.DIRECTORY_SEPARATOR.$prefix.rand_str(12);"\` |
|       - |  461 | `   "     if( file_exists($zPath) ){ continue; }"\` |
|       - |  462 | `   "     $pHandle = @fopen($zPath,'x');"\` |
|       - |  463 | `   "     if( $pHandle === false ){ continue; }"\` |
|       - |  464 | `   "     fclose($pHandle);"\` |
|       - |  465 | `   "     @chmod($zPath, 0600);"\` |
|       - |  466 | `   "     return $zPath;"\` |
|       - |  467 | `   "   }"\` |
|       - |  468 | `   "   return false;"\` |
|       - |  469 | `   "}"\` |
|       - |  470 | `	/* fileowner/filegroup/fileinode: see the note beside fileperms above. */\` |
|       - |  471 | `	""` |
|       - |  472 |  |
|       - |  473 | `/*` |
|       - |  474 | ` * ---------------------------------------------------------------------------` |
|       - |  475 | ` * The Exception / Error family, declared from C.` |
|       - |  476 | ` *` |
|       - |  477 | ` * php's two roots are one implementation twice over (its stub says` |
|       - |  478 | `` * `@implementation-alias Exception::__construct` for every one of Error's`` |
|       - |  479 | ` * methods), so the bodies below are shared by both spec tables and the` |
|       - |  480 | ` * ~20 subclasses are declaration-only rows.` |
|       - |  481 | ` *` |
|       - |  482 | `` * php's seven slots, in php's own declaration order. `string` is php's cache of`` |
|       - |  483 | ` * the __toString rendering -- unused by the engine but PRESENT on every` |
|       - |  484 | ` * presentation surface, which is why it is declared here rather than skipped:` |
|       - |  485 | ` * var_dump/print_r/(array)/serialize all show it, and PHL was one property short` |
|       - |  486 | ` * of php on every exception ever printed.` |
|       - |  487 | ` * ---------------------------------------------------------------------------` |
|       - |  488 | ` */` |
|       - |  489 | `#define EXC_MESSAGE  "message"` |
|       - |  490 | `#define EXC_STRING   "string"` |
|       - |  491 | `#define EXC_CODE     "code"` |
|       - |  492 | `#define EXC_FILE     "file"` |
|       - |  493 | `#define EXC_LINE     "line"` |
|       - |  494 | `#define EXC_TRACE    "trace"` |
|       - |  495 | `#define EXC_PREVIOUS "previous"` |
|       - |  496 | `#define EXC_SEVERITY "severity"` |
|       - |  497 | `/*` |
|       - |  498 | ` * Answer a declared slot the way php's getter does. Three of the seven CONVERT` |
|       - |  499 | ` * rather than copy — getMessage()/getFile() answer a string and getLine() an int,` |
|       - |  500 | ` * whatever the slot holds — and that shows twice: a subclass assigning` |
|       - |  501 | `` * `$this->message = 5` reads back "5", and a slot __wakeup has DROPPED reads as`` |
|       - |  502 | ` * "" rather than null. The other four are verbatim copies (getCode() of that same` |
|       - |  503 | ` * subclass really is the int).` |
|       - |  504 | ` */` |
|       - |  505 | `#define EXC_READ_RAW 0` |
|       - |  506 | `#define EXC_READ_STR 1` |
|       - |  507 | `#define EXC_READ_INT 2` |
|   15365 |  508 | `static int VmExcReadSlot(ph7_context *pCtx,const char *zSlot,int iAs)` |
|       5 |  509 | `{` |
|   15370 |  510 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   15370 |  511 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,zSlot) : 0;` |
|       - |  512 | `	ph7_value sTmp;` |
|   15370 |  513 | `	if( iAs == EXC_READ_RAW ){` |
|     507 |  514 | `		if( pVal ){` |
|     507 |  515 | `			ph7_result_value(pCtx,pVal);` |
|     256 |  516 | `		}else{` |
|     ! 0 |  517 | `			ph7_result_null(pCtx);` |
|       - |  518 | `		}` |
|     507 |  519 | `		return PH7_OK;` |
|       - |  520 | `	}` |
|       - |  521 | `	/* Through a COPY: converting the slot would rewrite the exception's state. */` |
|   14868 |  522 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|   14868 |  523 | `	if( pVal ){` |
|   14868 |  524 | `		PH7_MemObjStore(pVal,&sTmp);` |
|    7431 |  525 | `	}` |
|   14868 |  526 | `	if( iAs == EXC_READ_INT ){` |
|     631 |  527 | `		PH7_MemObjToInteger(&sTmp);` |
|     318 |  528 | `	}else{` |
|   14242 |  529 | `		PH7_MemObjToString(&sTmp);` |
|       - |  530 | `	}` |
|   14868 |  531 | `	ph7_result_value(pCtx,&sTmp);` |
|   14868 |  532 | `	PH7_MemObjRelease(&sTmp);` |
|   14868 |  533 | `	return PH7_OK;` |
|    7687 |  534 | `}` |
|   14223 |  535 | `static int vm_builtin_Exception_getMessage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  536 | `{` |
|    7111 |  537 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   14228 |  538 | `	return VmExcReadSlot(pCtx,EXC_MESSAGE,EXC_READ_STR);` |
|       5 |  539 | `}` |
|     468 |  540 | `static int vm_builtin_Exception_getCode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  541 | `{` |
|     234 |  542 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     471 |  543 | `	return VmExcReadSlot(pCtx,EXC_CODE,EXC_READ_RAW);` |
|       3 |  544 | `}` |
|      14 |  545 | `static int vm_builtin_Exception_getFile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  546 | `{` |
|       7 |  547 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      16 |  548 | `	return VmExcReadSlot(pCtx,EXC_FILE,EXC_READ_STR);` |
|       2 |  549 | `}` |
|     626 |  550 | `static int vm_builtin_Exception_getLine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  551 | `{` |
|     313 |  552 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     631 |  553 | `	return VmExcReadSlot(pCtx,EXC_LINE,EXC_READ_INT);` |
|       5 |  554 | `}` |
|       8 |  555 | `static int vm_builtin_Exception_getTrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  556 | `{` |
|       4 |  557 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      11 |  558 | `	return VmExcReadSlot(pCtx,EXC_TRACE,EXC_READ_RAW);` |
|       3 |  559 | `}` |
|      22 |  560 | `static int vm_builtin_Exception_getPrevious(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  561 | `{` |
|      11 |  562 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      25 |  563 | `	return VmExcReadSlot(pCtx,EXC_PREVIOUS,EXC_READ_RAW);` |
|       3 |  564 | `}` |
|       4 |  565 | `static int vm_builtin_ErrorException_getSeverity(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  566 | `{` |
|       2 |  567 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       5 |  568 | `	return VmExcReadSlot(pCtx,EXC_SEVERITY,EXC_READ_RAW);` |
|       1 |  569 | `}` |
|       - |  570 | `/*` |
|       - |  571 | ` * php's zend_update_exception_properties: each of the three is written only when` |
|       - |  572 | ``  * the caller actually supplied it — a message when the argument was PASSED (`""` `` |
|       - |  573 | ` * included), a code when it is NON-ZERO, a previous when it is an object. That is` |
|       - |  574 | ` * not the same as writing the defaults: a subclass may redeclare` |
|       - |  575 | `` * `protected $message = 'default'`, and php keeps it for `new Sub()`.`` |
|       - |  576 | ` */` |
| 1463675 |  577 | `static void VmExcInitProps(ph7_context *pCtx,ph7_class_instance *pThis,int nArg,` |
|       - |  578 | `	ph7_value **apArg,int iPrev)` |
|       5 |  579 | `{` |
| 1463680 |  580 | `	if( nArg > 0 ){` |
| 1463606 |  581 | `		int nMsg = 0;` |
| 1463606 |  582 | `		const char *zMsg = ph7_value_to_string(apArg[0],&nMsg);` |
| 1463606 |  583 | `		PH7_NativeSetAttrStr(pCtx->pVm,pThis,EXC_MESSAGE,zMsg,nMsg);` |
|  731800 |  584 | `	}` |
| 1463680 |  585 | `	if( nArg > 1 ){` |
|       - |  586 | `		ph7_value sCode;` |
|     458 |  587 | `		PH7_MemObjInit(pCtx->pVm,&sCode);` |
|     458 |  588 | `		PH7_MemObjStore(apArg[1],&sCode);` |
|     458 |  589 | `		PH7_MemObjToInteger(&sCode);` |
|     458 |  590 | `		if( sCode.x.iVal != 0 ){` |
|     436 |  591 | `			PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_CODE,sCode.x.iVal);` |
|     217 |  592 | `		}` |
|     458 |  593 | `		PH7_MemObjRelease(&sCode);` |
|     227 |  594 | `	}` |
|       - |  595 | ``	/* php's `previous` is the LAST parameter of each constructor, and`` |
|       - |  596 | `	 * ErrorException's is #5 rather than #2. */` |
| 1463680 |  597 | `	if( nArg > iPrev && (apArg[iPrev]->iFlags & MEMOBJ_OBJ) && apArg[iPrev]->x.pOther ){` |
|      24 |  598 | `		PH7_NativeSetAttrObj(pCtx->pVm,pThis,EXC_PREVIOUS,` |
|      14 |  599 | `			(ph7_class_instance *)apArg[iPrev]->x.pOther);` |
|       7 |  600 | `	}` |
| 1463680 |  601 | `}` |
| 1463661 |  602 | `static int vm_builtin_Exception_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  603 | `{` |
| 1463666 |  604 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
| 1463666 |  605 | `	if( pThis ){` |
| 1463666 |  606 | `		VmExcInitProps(pCtx,pThis,nArg,apArg,2);` |
|  731830 |  607 | `	}` |
| 1463666 |  608 | `	return PH7_OK;` |
|       5 |  609 | `}` |
|       - |  610 | `/*` |
|       - |  611 | ` * ErrorException's own constructor: php's Exception three, then severity, then` |
|       - |  612 | `` * the OPTIONAL file/line overrides. php's `?string $filename = null` /`` |
|       - |  613 | `` * `?int $line = null` mean "keep the creation site" — the chunk defaulted them to`` |
|       - |  614 | ` * __FILE__/__LINE__, which resolved against the EMBEDDED chunk and reported` |
|       - |  615 | `` * `:MEMORY:` line 1 for every ErrorException that did not pass them. php's one`` |
|       - |  616 | ` * asymmetry: a filename WITHOUT a line resets the line to 0.` |
|       - |  617 | ` */` |
|      14 |  618 | `static int vm_builtin_ErrorException_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  619 | `{` |
|      15 |  620 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      15 |  621 | `	if( pThis == 0 ){` |
|     ! 0 |  622 | `		return PH7_OK;` |
|       - |  623 | `	}` |
|      15 |  624 | `	VmExcInitProps(pCtx,pThis,nArg,apArg,5);` |
|      15 |  625 | `	if( nArg > 2 ){` |
|      11 |  626 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_SEVERITY,ph7_value_to_int64(apArg[2]));` |
|       5 |  627 | `	}` |
|      15 |  628 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|       9 |  629 | `		int nFile = 0;` |
|       9 |  630 | `		const char *zFile = ph7_value_to_string(apArg[3],&nFile);` |
|       9 |  631 | `		PH7_NativeSetAttrStr(pCtx->pVm,pThis,EXC_FILE,zFile,nFile);` |
|       9 |  632 | `		if( nArg < 5 \|\| ph7_value_is_null(apArg[4]) ){` |
|       3 |  633 | `			PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_LINE,0);` |
|       1 |  634 | `		}` |
|       4 |  635 | `	}` |
|      15 |  636 | `	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|       7 |  637 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_LINE,ph7_value_to_int64(apArg[4]));` |
|       3 |  638 | `	}` |
|      15 |  639 | `	return PH7_OK;` |
|       8 |  640 | `}` |
|       - |  641 | `/*` |
|       - |  642 | ` * php's private __clone. It has an empty body and is never reached: the class` |
|       - |  643 | ` * carries php's own clone refusal (PH7_CLASS_NOCLONE, answered before any body` |
|       - |  644 | `` * runs), which is what `clone $e` reports — "Trying to clone an uncloneable`` |
|       - |  645 | ` * object of class X", not a visibility error. Declaring it is still php-visible:` |
|       - |  646 | `` * Reflection lists it, and `$e->__clone()` from inside the class works.`` |
|       - |  647 | ` */` |
|     ! 0 |  648 | `static int vm_builtin_Exception_clone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  649 | `{` |
|     ! 0 |  650 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  651 | `	ph7_result_null(pCtx);` |
|     ! 0 |  652 | `	return PH7_OK;` |
|     ! 0 |  653 | `}` |
|       - |  654 | `/*` |
|       - |  655 | ` * php's __wakeup: the two UNTYPED slots are the only ones a serialized payload` |
|       - |  656 | ` * can lie about (the other five are typed and the store enforces them), so php` |
|       - |  657 | ` * DROPS a message that is not a string and a code that is not an int rather than` |
|       - |  658 | ` * letting a method read one.` |
|       - |  659 | ` */` |
|     ! 0 |  660 | `static void VmExcDropSlot(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot)` |
|     ! 0 |  661 | `{` |
|     ! 0 |  662 | `	SyHashEntry *pEntry = SyHashGet(&pThis->hAttr,(const void *)zSlot,SyStrlen(zSlot));` |
|     ! 0 |  663 | `	if( pEntry ){` |
|     ! 0 |  664 | `		PH7_VmReleaseInstanceAttr(&(*pVm),(VmClassAttr *)pEntry->pUserData);` |
|     ! 0 |  665 | `		SyHashDeleteEntry2(pEntry);` |
|     ! 0 |  666 | `	}` |
|     ! 0 |  667 | `}` |
|     ! 0 |  668 | `static int vm_builtin_Exception_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  669 | `{` |
|     ! 0 |  670 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - |  671 | `	ph7_value *pVal;` |
|     ! 0 |  672 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  673 | `	if( pThis == 0 ){` |
|     ! 0 |  674 | `		return PH7_OK;` |
|       - |  675 | `	}` |
|     ! 0 |  676 | `	pVal = PH7_NativeAttr(pThis,EXC_MESSAGE);` |
|     ! 0 |  677 | `	if( pVal && (pVal->iFlags & MEMOBJ_NULL) == 0 && (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 |  678 | `		VmExcDropSlot(pCtx->pVm,pThis,EXC_MESSAGE);` |
|     ! 0 |  679 | `	}` |
|     ! 0 |  680 | `	pVal = PH7_NativeAttr(pThis,EXC_CODE);` |
|     ! 0 |  681 | `	if( pVal && (pVal->iFlags & MEMOBJ_NULL) == 0 && (pVal->iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 |  682 | `		VmExcDropSlot(pCtx->pVm,pThis,EXC_CODE);` |
|     ! 0 |  683 | `	}` |
|     ! 0 |  684 | `	ph7_result_null(pCtx);` |
|     ! 0 |  685 | `	return PH7_OK;` |
|     ! 0 |  686 | `}` |
|       - |  687 | `/*` |
|       - |  688 | ` * One argument of a trace frame, php's smart_str_append_scalar: a string is` |
|       - |  689 | `` * single-quoted, ESCAPED (`\n`, `\xNN` for anything non-printable) and truncated`` |
|       - |  690 | `` * to 15 bytes with `...` inside the quotes; a float takes php's precision; an`` |
|       - |  691 | `` * enum case prints `Enum::Case`; and anything else is a bare word.`` |
|       - |  692 | ` */` |
|       - |  693 | `#define EXC_ARG_MAX 15` |
|      62 |  694 | `static void VmExcTraceArg(ph7_vm *pVm,SyBlob *pOut,ph7_value *pArg)` |
|       1 |  695 | `{` |
|      63 |  696 | `	if( pArg == 0 \|\| (pArg->iFlags & MEMOBJ_NULL) ){` |
|       3 |  697 | `		SyBlobAppend(pOut,"NULL",sizeof("NULL")-1);` |
|       3 |  698 | `		return;` |
|       - |  699 | `	}` |
|      61 |  700 | `	if( pArg->iFlags & MEMOBJ_BOOL ){` |
|       5 |  701 | `		if( pArg->x.iVal ){` |
|       3 |  702 | `			SyBlobAppend(pOut,"true",sizeof("true")-1);` |
|       2 |  703 | `		}else{` |
|       3 |  704 | `			SyBlobAppend(pOut,"false",sizeof("false")-1);` |
|       - |  705 | `		}` |
|       5 |  706 | `		return;` |
|       - |  707 | `	}` |
|      57 |  708 | `	if( pArg->iFlags & MEMOBJ_HASHMAP ){` |
|       3 |  709 | `		SyBlobAppend(pOut,"Array",sizeof("Array")-1);` |
|       3 |  710 | `		return;` |
|       - |  711 | `	}` |
|      55 |  712 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|       3 |  713 | `		ph7_class_instance *pObj = (ph7_class_instance *)pArg->x.pOther;` |
|       3 |  714 | `		if( pObj && pObj->pClass && (pObj->pClass->iFlags & PH7_CLASS_ENUM) ){` |
|     ! 0 |  715 | `			ph7_value *pName = PH7_NativeAttr(pObj,"name");` |
|     ! 0 |  716 | `			SyBlobFormat(pOut,"%z::",&pObj->pClass->sName);` |
|     ! 0 |  717 | `			if( pName ){` |
|     ! 0 |  718 | `				SyBlobAppend(pOut,SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|     ! 0 |  719 | `			}` |
|     ! 0 |  720 | `			return;` |
|       - |  721 | `		}` |
|       3 |  722 | `		SyBlobAppend(pOut,"Object(",sizeof("Object(")-1);` |
|       3 |  723 | `		if( pObj && pObj->pClass ){` |
|       3 |  724 | `			SyBlobFormat(pOut,"%z",&pObj->pClass->sName);` |
|       1 |  725 | `		}` |
|       3 |  726 | `		SyBlobAppend(pOut,")",sizeof(")")-1);` |
|       3 |  727 | `		return;` |
|       - |  728 | `	}` |
|      53 |  729 | `	if( pArg->iFlags & MEMOBJ_STRING ){` |
|       - |  730 | `		/* php 8.5 does not put string CONTENT in a trace at all: every non-empty` |
|       - |  731 | `		 * one renders as '...' (the empty one still shows as ''), so a password` |
|       - |  732 | `		 * or a token passed to the function that threw cannot reach a log through` |
|       - |  733 | `		 * the trace. The truncate-at-15-and-escape shape here was php 8.4's. */` |
|       7 |  734 | `		if( SyBlobLength(&pArg->sBlob) < 1 ){` |
|       3 |  735 | `			SyBlobAppend(pOut,"''",sizeof("''")-1);` |
|       2 |  736 | `		}else{` |
|       5 |  737 | `			SyBlobAppend(pOut,"'...'",sizeof("'...'")-1);` |
|       - |  738 | `		}` |
|       7 |  739 | `		return;` |
|       - |  740 | `	}` |
|       - |  741 | `	{` |
|       - |  742 | `		/* int / float / anything else: php prints the scalar itself -- but a` |
|       - |  743 | `		 * trace FLOAT always shows its fraction (1.0, not the "1" the ordinary` |
|       - |  744 | `		 * string cast produces), which is what tells a float argument apart from` |
|       - |  745 | `		 * an int one. INF/NAN and the exponent forms already carry a marker. */` |
|       - |  746 | `		ph7_value sTmp;` |
|       - |  747 | `		const char *z;` |
|       - |  748 | `		sxu32 n,i;` |
|      47 |  749 | `		int bMarked = 0;` |
|      47 |  750 | `		PH7_MemObjInit(&(*pVm),&sTmp);` |
|      47 |  751 | `		PH7_MemObjStore(pArg,&sTmp);` |
|      47 |  752 | `		PH7_MemObjToString(&sTmp);` |
|      47 |  753 | `		z = (const char *)SyBlobData(&sTmp.sBlob);` |
|      47 |  754 | `		n = SyBlobLength(&sTmp.sBlob);` |
|      47 |  755 | `		SyBlobAppend(pOut,z,n);` |
|      47 |  756 | `		if( pArg->iFlags & MEMOBJ_REAL ){` |
|      23 |  757 | `			for( i = 0 ; i < n ; ++i ){` |
|      19 |  758 | `				if( z[i] < '0' \|\| z[i] > '9' ){` |
|      11 |  759 | `					if( z[i] != '-' && z[i] != '+' ){` |
|       9 |  760 | `						bMarked = 1;` |
|       9 |  761 | `						break;` |
|       - |  762 | `					}` |
|       1 |  763 | `				}` |
|       6 |  764 | `			}` |
|      13 |  765 | `			if( !bMarked ){` |
|       5 |  766 | `				SyBlobAppend(pOut,".0",sizeof(".0")-1);` |
|       2 |  767 | `			}` |
|       6 |  768 | `		}` |
|      47 |  769 | `		PH7_MemObjRelease(&sTmp);` |
|       - |  770 | `	}` |
|      32 |  771 | `}` |
|       - |  772 | `/* An element of a trace frame, or NULL when the frame does not carry it. */` |
|     612 |  773 | `static ph7_value * VmExcFrameField(ph7_vm *pVm,ph7_hashmap *pFrame,const char *zField)` |
|       5 |  774 | `{` |
|     617 |  775 | `	ph7_hashmap_node *pNode = 0;` |
|       - |  776 | `	ph7_value sKey;` |
|       - |  777 | `	sxi32 rc;` |
|       - |  778 | `	SyString sName;` |
|     617 |  779 | `	SyStringInitFromBuf(&sName,zField,SyStrlen(zField));` |
|     617 |  780 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,&sName);` |
|     617 |  781 | `	rc = PH7_HashmapLookup(pFrame,&sKey,&pNode);` |
|     617 |  782 | `	PH7_MemObjRelease(&sKey);` |
|     617 |  783 | `	if( rc != SXRET_OK \|\| pNode == 0 ){` |
|     223 |  784 | `		return 0;` |
|       - |  785 | `	}` |
|     399 |  786 | `	return (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|     311 |  787 | `}` |
|     414 |  788 | `static void VmExcFrameStr(SyBlob *pOut,ph7_value *pVal)` |
|       5 |  789 | `{` |
|     419 |  790 | `	if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){` |
|     255 |  791 | `		SyBlobAppend(pOut,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|     125 |  792 | `	}` |
|     419 |  793 | `}` |
|       - |  794 | `/* A slot's string form, taken through a COPY: converting the value in place` |
|       - |  795 | ` * would rewrite the exception's own state. */` |
|       6 |  796 | `static void VmExcValueStr(ph7_vm *pVm,ph7_value *pVal,SyBlob *pOut)` |
|       1 |  797 | `{` |
|       - |  798 | `	ph7_value sTmp;` |
|       7 |  799 | `	if( pVal == 0 ){` |
|     ! 0 |  800 | `		return;` |
|       - |  801 | `	}` |
|       7 |  802 | `	PH7_MemObjInit(&(*pVm),&sTmp);` |
|       7 |  803 | `	PH7_MemObjStore(pVal,&sTmp);` |
|       7 |  804 | `	PH7_MemObjToString(&sTmp);` |
|       7 |  805 | `	SyBlobAppend(pOut,SyBlobData(&sTmp.sBlob),SyBlobLength(&sTmp.sBlob));` |
|       7 |  806 | `	PH7_MemObjRelease(&sTmp);` |
|       4 |  807 | `}` |
|       - |  808 | ``/* Does the blob contain this literal? SyBlobSearch() is `#ifndef`` |
|       - |  809 | `` * PH7_DISABLE_BUILTIN_FUNC`, and the exception family exists in the tiny build`` |
|       - |  810 | ` * too, so the one search this file needs is spelled out. */` |
|     ! 0 |  811 | `static int VmExcBlobHas(SyBlob *pBlob,const char *zPat,sxu32 nPat)` |
|     ! 0 |  812 | `{` |
|     ! 0 |  813 | `	const char *z = (const char *)SyBlobData(pBlob);` |
|     ! 0 |  814 | `	sxu32 n = SyBlobLength(pBlob);` |
|       - |  815 | `	sxu32 i;` |
|     ! 0 |  816 | `	if( nPat == 0 \|\| n < nPat ){` |
|     ! 0 |  817 | `		return 0;` |
|       - |  818 | `	}` |
|     ! 0 |  819 | `	for( i = 0 ; i + nPat <= n ; i++ ){` |
|     ! 0 |  820 | `		if( SyMemcmp((const void *)&z[i],(const void *)zPat,nPat) == 0 ){` |
|     ! 0 |  821 | `			return 1;` |
|       - |  822 | `		}` |
|     ! 0 |  823 | `	}` |
|     ! 0 |  824 | `	return 0;` |
|     ! 0 |  825 | `}` |
|       - |  826 | ``/* php's `Z_OBJCE_P == zend_ce_type_error \|\| == zend_ce_argument_count_error`:`` |
|       - |  827 | ` * the two classes whose message __toString finishes with " and defined". */` |
|       6 |  828 | `static int VmExcIsArgError(ph7_vm *pVm,ph7_class_instance *pExc)` |
|       1 |  829 | `{` |
|       7 |  830 | `	ph7_class *pClass = pExc ? pExc->pClass : 0;` |
|       - |  831 | `	ph7_class *pType;` |
|       7 |  832 | `	if( pClass == 0 ){` |
|     ! 0 |  833 | `		return 0;` |
|       - |  834 | `	}` |
|       7 |  835 | `	pType = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,FALSE,0);` |
|       7 |  836 | `	if( pType && pClass == pType ){` |
|     ! 0 |  837 | `		return 1;` |
|       - |  838 | `	}` |
|       7 |  839 | `	pType = PH7_VmExtractClass(&(*pVm),"ArgumentCountError",sizeof("ArgumentCountError")-1,FALSE,0);` |
|       7 |  840 | `	return pType != 0 && pClass == pType;` |
|       4 |  841 | `}` |
|       - |  842 | `/*` |
|       - |  843 | `` * php's zend_trace_to_string: one `#N file(line): Class->method(args)` line per`` |
|       - |  844 | `` * frame, then `#N {main}` with NO trailing newline. A frame with no `file` is`` |
|       - |  845 | `` * php's `[internal function]: `.`` |
|       - |  846 | ` */` |
|     650 |  847 | `PH7_PRIVATE void PH7_VmTraceToString(ph7_vm *pVm,ph7_value *pTrace,int bMainMarker,SyBlob *pOut)` |
|       5 |  848 | `{` |
|       - |  849 | `	ph7_hashmap *pMap;` |
|       - |  850 | `	ph7_hashmap_node *pEntry;` |
|     655 |  851 | `	sxu32 nFrame = 0;` |
|     655 |  852 | `	if( pTrace && (pTrace->iFlags & MEMOBJ_HASHMAP) && pTrace->x.pOther ){` |
|     655 |  853 | `		pMap = (ph7_hashmap *)pTrace->x.pOther;` |
|       - |  854 | `		/* Insertion order is pFirst then the pPrev chain (rule 12). */` |
|     757 |  855 | `		for( pEntry = pMap->pFirst ; pEntry ; pEntry = pEntry->pPrev ){` |
|     107 |  856 | `			ph7_value *pFrameVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|       - |  857 | `			ph7_hashmap *pFrame;` |
|       - |  858 | `			ph7_value *pFile;` |
|     107 |  859 | `			if( pFrameVal == 0 \|\| (pFrameVal->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 |  860 | `				continue;` |
|       - |  861 | `			}` |
|     107 |  862 | `			pFrame = (ph7_hashmap *)pFrameVal->x.pOther;` |
|     107 |  863 | `			SyBlobFormat(pOut,"#%u ",nFrame);` |
|     107 |  864 | `			pFile = VmExcFrameField(&(*pVm),pFrame,"file");` |
|     158 |  865 | `			if( pFile && (pFile->iFlags & MEMOBJ_STRING) ){` |
|     107 |  866 | `				ph7_value *pLine = VmExcFrameField(&(*pVm),pFrame,"line");` |
|     107 |  867 | `				VmExcFrameStr(pOut,pFile);` |
|     209 |  868 | `				SyBlobFormat(pOut,"(%qd): ",` |
|     102 |  869 | `					(pLine && (pLine->iFlags & MEMOBJ_INT)) ? pLine->x.iVal : (sxi64)0);` |
|      56 |  870 | `			}else{` |
|     ! 0 |  871 | `				SyBlobAppend(pOut,"[internal function]: ",sizeof("[internal function]: ")-1);` |
|       - |  872 | `			}` |
|     107 |  873 | `			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"class"));` |
|     107 |  874 | `			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"type"));` |
|     107 |  875 | `			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"function"));` |
|     107 |  876 | `			SyBlobAppend(pOut,"(",sizeof("(")-1);` |
|       - |  877 | `			{` |
|     107 |  878 | `				ph7_value *pArgs = VmExcFrameField(&(*pVm),pFrame,"args");` |
|     107 |  879 | `				if( pArgs && (pArgs->iFlags & MEMOBJ_HASHMAP) && pArgs->x.pOther ){` |
|      49 |  880 | `					ph7_hashmap *pArgMap = (ph7_hashmap *)pArgs->x.pOther;` |
|       - |  881 | `					ph7_hashmap_node *pArg;` |
|      49 |  882 | `					int bFirst = 1;` |
|     111 |  883 | `					for( pArg = pArgMap->pFirst ; pArg ; pArg = pArg->pPrev ){` |
|      63 |  884 | `						if( !bFirst ){` |
|      17 |  885 | `							SyBlobAppend(pOut,", ",sizeof(", ")-1);` |
|       8 |  886 | `						}` |
|      63 |  887 | `						bFirst = 0;` |
|      94 |  888 | `						VmExcTraceArg(&(*pVm),pOut,` |
|      62 |  889 | `							(ph7_value *)SySetAt(&pVm->aMemObj,pArg->nValIdx));` |
|      32 |  890 | `					}` |
|      24 |  891 | `				}` |
|       - |  892 | `			}` |
|     107 |  893 | `			SyBlobAppend(pOut,")\n",sizeof(")\n")-1);` |
|     107 |  894 | `			nFrame++;` |
|      56 |  895 | `		}` |
|     325 |  896 | `	}` |
|     655 |  897 | `	if( bMainMarker ){` |
|       - |  898 | `		/* getTraceAsString() ends on the bottom marker; debug_print_backtrace()` |
|       - |  899 | `		 * does not print one -- it stops after the last real frame. */` |
|     613 |  900 | `		SyBlobFormat(pOut,"#%u {main}",nFrame);` |
|     304 |  901 | `	}` |
|     655 |  902 | `}` |
|     602 |  903 | `static int vm_builtin_Exception_getTraceAsString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  904 | `{` |
|     607 |  905 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - |  906 | `	SyBlob sOut;` |
|     301 |  907 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     607 |  908 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     607 |  909 | `	PH7_VmTraceToString(pCtx->pVm,pThis ? PH7_NativeAttr(pThis,EXC_TRACE) : 0,TRUE,&sOut);` |
|     607 |  910 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     607 |  911 | `	SyBlobRelease(&sOut);` |
|     607 |  912 | `	return PH7_OK;` |
|       5 |  913 | `}` |
|       - |  914 | `/*` |
|       - |  915 | ` * php's Exception::__toString.` |
|       - |  916 | ` *` |
|       - |  917 | ` *    C: message in file:line` |
|       - |  918 | ` *    Stack trace:` |
|       - |  919 | ` *    <trace>` |
|       - |  920 | ` *` |
|       - |  921 | ` * The PREVIOUS chain is part of the format and the ORDER is inverted: php builds` |
|       - |  922 | `` * the string innermost-first and joins the shallower ones after `\n\nNext `, so`` |
|       - |  923 | ` * the root cause is printed first. The chunk answered a four-field space-joined` |
|       - |  924 | `` * line instead — `file line code message` — which no php ever produced, and it is`` |
|       - |  925 | `` * what an uncaught exception, `echo $e` and `(string)$e` all show.`` |
|       - |  926 | ` *` |
|       - |  927 | ` * The walk carries its ancestors on the C stack (rule 31): php protects each` |
|       - |  928 | `` * object it visits and stops when it comes back round, and a `$a->previous = $b;`` |
|       - |  929 | `` * $b->previous = $a` pair must not spin.`` |
|       - |  930 | ` */` |
|       - |  931 | `#define EXC_CHAIN_MAX 256` |
|       4 |  932 | `static int vm_builtin_Exception_toString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  933 | `{` |
|       - |  934 | `	ph7_class_instance *apChain[EXC_CHAIN_MAX];` |
|       5 |  935 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       5 |  936 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - |  937 | `	SyBlob sOut;` |
|       5 |  938 | `	int nChain = 0;` |
|       - |  939 | `	int i,j;` |
|       2 |  940 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      11 |  941 | `	while( pThis && nChain < EXC_CHAIN_MAX ){` |
|       - |  942 | `		ph7_class_instance *pPrev;` |
|       9 |  943 | `		for( j = 0 ; j < nChain ; j++ ){` |
|       3 |  944 | `			if( apChain[j] == pThis ){` |
|     ! 0 |  945 | `				pThis = 0;    /* already on the chain: php's recursion protection */` |
|     ! 0 |  946 | `				break;` |
|       - |  947 | `			}` |
|       2 |  948 | `		}` |
|       7 |  949 | `		if( pThis == 0 ){` |
|     ! 0 |  950 | `			break;` |
|       - |  951 | `		}` |
|       7 |  952 | `		apChain[nChain++] = pThis;` |
|       7 |  953 | `		pPrev = PH7_NativeAttrObj(pThis,EXC_PREVIOUS);` |
|       7 |  954 | `		pThis = pPrev;` |
|       1 |  955 | `	}` |
|       - |  956 | `	/* php formats the SHALLOWEST first and pushes each one it has already built` |
|       - |  957 | `	 * behind the next, so the printed order is inverted: the ROOT CAUSE leads and` |
|       - |  958 | ``	 * every caller follows it after `\n\nNext `. */`` |
|       5 |  959 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      11 |  960 | `	for( i = 0 ; i < nChain ; i++ ){` |
|       7 |  961 | `		ph7_class_instance *pExc = apChain[i];` |
|       7 |  962 | `		ph7_value *pLine = PH7_NativeAttr(pExc,EXC_LINE);` |
|       - |  963 | `		SyBlob sMsg;` |
|       - |  964 | `		SyBlob sThis;` |
|       7 |  965 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       7 |  966 | `		SyBlobInit(&sThis,&pVm->sAllocator);` |
|       7 |  967 | `		VmExcValueStr(pVm,PH7_NativeAttr(pExc,EXC_MESSAGE),&sMsg);` |
|       - |  968 | `		/* php's one message rewrite: a TypeError/ArgumentCountError raised at a` |
|       - |  969 | `		 * CALL SITE says "..., called in F on line N", and __toString finishes the` |
|       - |  970 | `		 * sentence with " and defined". */` |
|       6 |  971 | `		if( VmExcIsArgError(pVm,pExc)` |
|       4 |  972 | `		 && VmExcBlobHas(&sMsg,", called in ",sizeof(", called in ")-1) ){` |
|     ! 0 |  973 | `			SyBlobAppend(&sMsg," and defined",sizeof(" and defined")-1);` |
|     ! 0 |  974 | `		}` |
|       7 |  975 | `		SyBlobFormat(&sThis,"%z",&pExc->pClass->sName);` |
|       7 |  976 | `		if( SyBlobLength(&sMsg) > 0 ){` |
|       5 |  977 | `			SyBlobAppend(&sThis,": ",sizeof(": ")-1);` |
|       5 |  978 | `			SyBlobAppend(&sThis,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       2 |  979 | `		}` |
|       7 |  980 | `		SyBlobAppend(&sThis," in ",sizeof(" in ")-1);` |
|       7 |  981 | `		VmExcFrameStr(&sThis,PH7_NativeAttr(pExc,EXC_FILE));` |
|      10 |  982 | `		SyBlobFormat(&sThis,":%qd\nStack trace:\n",` |
|       6 |  983 | `			(pLine && (pLine->iFlags & MEMOBJ_INT)) ? pLine->x.iVal : (sxi64)0);` |
|       7 |  984 | `		PH7_VmTraceToString(pVm,PH7_NativeAttr(pExc,EXC_TRACE),TRUE,&sThis);` |
|       7 |  985 | `		if( SyBlobLength(&sOut) > 0 ){` |
|       3 |  986 | `			SyBlobAppend(&sThis,"\n\nNext ",sizeof("\n\nNext ")-1);` |
|       3 |  987 | `			SyBlobAppend(&sThis,SyBlobData(&sOut),SyBlobLength(&sOut));` |
|       1 |  988 | `		}` |
|       7 |  989 | `		SyBlobReset(&sOut);` |
|       7 |  990 | `		SyBlobAppend(&sOut,SyBlobData(&sThis),SyBlobLength(&sThis));` |
|       7 |  991 | `		SyBlobRelease(&sMsg);` |
|       7 |  992 | `		SyBlobRelease(&sThis);` |
|       4 |  993 | `	}` |
|       5 |  994 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|       5 |  995 | `	SyBlobRelease(&sOut);` |
|       5 |  996 | `	return PH7_OK;` |
|       1 |  997 | `}` |
|       - |  998 | `/*` |
|       - |  999 | ` * The declaration. php's two roots carry the same eleven methods and the same` |
|       - | 1000 | `` * seven slots; the only difference php's stub records is `Error::$line`, which`` |
|       - | 1001 | ` * has NO default where Exception's is 0.` |
|       - | 1002 | ` *` |
|       - | 1003 | `` * PH7_CLASS_NOCLONE on EVERY row: php refuses `clone $e` outright, and a native`` |
|       - | 1004 | ` * subclass does not inherit its parent's class flags (rule 29).` |
|       - | 1005 | ` */` |
|       - | 1006 | `#define EXC_METHODS(zCtor,xCtor) \` |
|       - | 1007 | `	{ "__clone",          PH7_MOD_PRIVATE, "", "void", vm_builtin_Exception_clone }, \` |
|       - | 1008 | `	{ "__construct",      PH7_MOD_PUBLIC, zCtor, 0, xCtor }, \` |
|       - | 1009 | `	{ "__wakeup",         PH7_MOD_PUBLIC, "", "@void", vm_builtin_Exception_wakeup }, \` |
|       - | 1010 | `	{ "getMessage",       PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "string", \` |
|       - | 1011 | `	  vm_builtin_Exception_getMessage }, \` |
|       - | 1012 | `	{ "getCode",          PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", 0, \` |
|       - | 1013 | `	  vm_builtin_Exception_getCode }, \` |
|       - | 1014 | `	{ "getFile",          PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "string", \` |
|       - | 1015 | `	  vm_builtin_Exception_getFile }, \` |
|       - | 1016 | `	{ "getLine",          PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "int", \` |
|       - | 1017 | `	  vm_builtin_Exception_getLine }, \` |
|       - | 1018 | `	{ "getTrace",         PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "array", \` |
|       - | 1019 | `	  vm_builtin_Exception_getTrace }, \` |
|       - | 1020 | `	{ "getPrevious",      PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "?Throwable", \` |
|       - | 1021 | `	  vm_builtin_Exception_getPrevious }, \` |
|       - | 1022 | `	{ "getTraceAsString", PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "string", \` |
|       - | 1023 | `	  vm_builtin_Exception_getTraceAsString }, \` |
|       - | 1024 | `	{ "__toString",       PH7_MOD_PUBLIC, "", "string", vm_builtin_Exception_toString }` |
|       - | 1025 | `#define EXC_CTOR_SIG "string $message = \"\", int $code = 0, ?Throwable $previous = null"` |
|       - | 1026 | `/* php's seven slots, twice: the only difference between the two roots is` |
|       - | 1027 | `` * `Error::$line`, which php's stub declares with NO default where Exception's is`` |
|       - | 1028 | `` * 0 (`PH7_NATIVE_VAL_NONE` — its hasDefaultValue() is false and the export`` |
|       - | 1029 | `` * prints `protected int $line` bare). `message` and `code` are the two php leaves`` |
|       - | 1030 | ` * UNTYPED, and its stub says why: BC, since a subclass may have assigned` |
|       - | 1031 | ` * anything to them. */` |
|       - | 1032 | `#define EXC_PROP_HEAD \` |
|       - | 1033 | `	{ EXC_MESSAGE,  PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 }, \` |
|       - | 1034 | `	{ EXC_STRING,   PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" }, \` |
|       - | 1035 | `	{ EXC_CODE,     PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 }, \` |
|       - | 1036 | `	{ EXC_FILE,     PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" }` |
|       - | 1037 | `#define EXC_PROP_TAIL \` |
|       - | 1038 | `	{ EXC_TRACE,    PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_ARRAY, 0, 0, 0.0 }, "array" }, \` |
|       - | 1039 | `	{ EXC_PREVIOUS, PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?Throwable" }` |
|    5740 | 1040 | `static sxi32 VmInstallExceptions(ph7_vm *pVm)` |
|       5 | 1041 | `{` |
|       - | 1042 | `	static const PH7_NativeMethodDef aExcMethod[] = {` |
|       - | 1043 | `		EXC_METHODS(EXC_CTOR_SIG,vm_builtin_Exception_construct)` |
|       - | 1044 | `	};` |
|       - | 1045 | `	static const PH7_NativePropDef aExcProp[] = {` |
|       - | 1046 | `		EXC_PROP_HEAD,` |
|       - | 1047 | `		{ EXC_LINE, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, "int" },` |
|       - | 1048 | `		EXC_PROP_TAIL` |
|       - | 1049 | `	};` |
|       - | 1050 | `	static const PH7_NativePropDef aErrProp[] = {` |
|       - | 1051 | `		EXC_PROP_HEAD,` |
|       - | 1052 | `		{ EXC_LINE, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|       - | 1053 | `		EXC_PROP_TAIL` |
|       - | 1054 | `	};` |
|       - | 1055 | `	static const PH7_NativePropDef aErrExcProp[] = {` |
|       - | 1056 | `		{ EXC_SEVERITY, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 1, 0, 0.0 }, "int" },` |
|       - | 1057 | `	};` |
|       - | 1058 | `	static const PH7_NativeMethodDef aErrExcMethod[] = {` |
|       - | 1059 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|       - | 1060 | `		  "string $message = \"\", int $code = 0, int $severity = E_ERROR, "` |
|       - | 1061 | `		  "?string $filename = null, ?int $line = null, ?Throwable $previous = null", 0,` |
|       - | 1062 | `		  vm_builtin_ErrorException_construct },` |
|       - | 1063 | `		{ "getSeverity", PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "int",` |
|       - | 1064 | `		  vm_builtin_ErrorException_getSeverity },` |
|       - | 1065 | `	};` |
|       - | 1066 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1067 | `		{ "Exception", 0, "Throwable", PH7_CLASS_NOCLONE,` |
|       - | 1068 | `		  aExcMethod, SX_ARRAYSIZE(aExcMethod), 0, 0, aExcProp, SX_ARRAYSIZE(aExcProp), 0, 0, 0 },` |
|       - | 1069 | `		{ "Error", 0, "Throwable", PH7_CLASS_NOCLONE,` |
|       - | 1070 | `		  aExcMethod, SX_ARRAYSIZE(aExcMethod), 0, 0, aErrProp, SX_ARRAYSIZE(aErrProp), 0, 0, 0 },` |
|       - | 1071 | `		/* Zend's own subclasses, then ErrorException, then SPL's tree. Every row is` |
|       - | 1072 | `		 * declaration-only in php too. */` |
|       - | 1073 | `		{ "TypeError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1074 | `		{ "ArgumentCountError", "TypeError", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1075 | `		{ "ValueError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1076 | `		{ "FiberError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1077 | `		{ "AssertionError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1078 | `		{ "ArithmeticError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1079 | `		{ "DivisionByZeroError", "ArithmeticError", 0, PH7_CLASS_NOCLONE,` |
|       - | 1080 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1081 | `		{ "UnhandledMatchError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1082 | `		{ "CompileError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1083 | `		{ "ParseError", "CompileError", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1084 | `		{ "ErrorException", "Exception", 0, PH7_CLASS_NOCLONE,` |
|       - | 1085 | `		  aErrExcMethod, SX_ARRAYSIZE(aErrExcMethod), 0, 0,` |
|       - | 1086 | `		  aErrExcProp, SX_ARRAYSIZE(aErrExcProp), 0, 0, 0 },` |
|       - | 1087 | `		{ "LogicException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1088 | `		{ "RuntimeException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1089 | `		{ "BadFunctionCallException", "LogicException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1090 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1091 | `		{ "BadMethodCallException", "BadFunctionCallException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1092 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1093 | `		{ "DomainException", "LogicException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1094 | `		{ "InvalidArgumentException", "LogicException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1095 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1096 | `		{ "LengthException", "LogicException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1097 | `		{ "OutOfRangeException", "LogicException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1098 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1099 | `		{ "OutOfBoundsException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1100 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1101 | `		{ "OverflowException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1102 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1103 | `		{ "RangeException", "RuntimeException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1104 | `		{ "UnderflowException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1105 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1106 | `		{ "UnexpectedValueException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1107 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1108 | `		{ "JsonException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1109 | `	};` |
|    5745 | 1110 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       5 | 1111 | `}` |
|       - | 1112 | `/*` |
|       - | 1113 | ` * The eleven core interfaces, declared from C.` |
|       - | 1114 | ` *` |
|       - | 1115 | ` * They are contracts -- no method here has a body, every row is` |
|       - | 1116 | ` * PH7_MOD_ABSTRACT -- so the conversion is entirely about what the DECLARATION` |
|       - | 1117 | ` * says, which is where a chunk fell short in four php-visible ways:` |
|       - | 1118 | ` *` |
|       - | 1119 | `` *  - php's `interface Throwable extends Stringable`: the chunk redeclared`` |
|       - | 1120 | ` *    __toString() on Throwable instead, so no Exception was ever Stringable` |
|       - | 1121 | `` *    (`$e instanceof Stringable` was false, and Reflection attributed the`` |
|       - | 1122 | `` *    method to Throwable rather than printing php's `inherits Stringable`);`` |
|       - | 1123 | ` *  - php declares a RETURN TYPE on all but three of these methods and marks` |
|       - | 1124 | `` *    nearly all of them TENTATIVE (the leading `@`, rule 45) -- a chunk has no`` |
|       - | 1125 | ` *    way to say tentative at all;` |
|       - | 1126 | `` *  - php's `mixed` on ArrayAccess's offsets, which the chunk left untyped;`` |
|       - | 1127 | ` *  - method ORDER, which Reflection prints: php lists Throwable's getPrevious` |
|       - | 1128 | ` *    before getTraceAsString, and Iterator's as current/next/key/valid/rewind.` |
|       - | 1129 | ` *` |
|       - | 1130 | ` * Order within the table is php's stub order too; the declare-then-link phases` |
|       - | 1131 | ` * of PH7_InstallNativeClasses let Throwable name Stringable and Iterator name` |
|       - | 1132 | `` * Traversable regardless of row order. An interface's parent is `zParent`, not`` |
|       - | 1133 | `` * `zImplements` (Reflection walks pBase to attribute an inherited method).`` |
|       - | 1134 | ` */` |
|    5740 | 1135 | `static sxi32 VmInstallCoreInterfaces(ph7_vm *pVm)` |
|       5 | 1136 | `{` |
|       - | 1137 | `	static const PH7_NativeMethodDef aStringable[] = {` |
|       - | 1138 | `		{ "__toString", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1139 | `	};` |
|       - | 1140 | `	static const PH7_NativeMethodDef aThrowable[] = {` |
|       - | 1141 | `		/* Not one of these is tentative: php's Throwable is a real contract. */` |
|       - | 1142 | `		{ "getMessage",       PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1143 | `		{ "getCode",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|       - | 1144 | `		{ "getFile",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1145 | `		{ "getLine",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "int", 0 },` |
|       - | 1146 | `		{ "getTrace",         PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "array", 0 },` |
|       - | 1147 | `		{ "getPrevious",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "?Throwable", 0 },` |
|       - | 1148 | `		{ "getTraceAsString", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1149 | `	};` |
|       - | 1150 | `	static const PH7_NativeMethodDef aArrayAccess[] = {` |
|       - | 1151 | `		{ "offsetExists", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset", "@bool", 0 },` |
|       - | 1152 | `		{ "offsetGet",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset", "@mixed", 0 },` |
|       - | 1153 | `		{ "offsetSet",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset, mixed $value",` |
|       - | 1154 | `		  "@void", 0 },` |
|       - | 1155 | `		{ "offsetUnset",  PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset", "@void", 0 },` |
|       - | 1156 | `	};` |
|       - | 1157 | `	static const PH7_NativeMethodDef aCountable[] = {` |
|       - | 1158 | `		{ "count", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@int", 0 },` |
|       - | 1159 | `	};` |
|       - | 1160 | `	static const PH7_NativeMethodDef aJsonSerializable[] = {` |
|       - | 1161 | `		{ "jsonSerialize", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@mixed", 0 },` |
|       - | 1162 | `	};` |
|       - | 1163 | `	/* The concrete cases()/from()/tryFrom() an enum gets are native methods` |
|       - | 1164 | `	 * declared to match these (oo_native.c, PH7_InstallEnumInterfaceMethods). */` |
|       - | 1165 | `	static const PH7_NativeMethodDef aUnitEnum[] = {` |
|       - | 1166 | `		{ "cases", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_ABSTRACT, "", "array", 0 },` |
|       - | 1167 | `	};` |
|       - | 1168 | `	static const PH7_NativeMethodDef aBackedEnum[] = {` |
|       - | 1169 | `		{ "from",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_ABSTRACT, "string\|int $value",` |
|       - | 1170 | `		  "static", 0 },` |
|       - | 1171 | `		{ "tryFrom", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_ABSTRACT, "string\|int $value",` |
|       - | 1172 | `		  "?static", 0 },` |
|       - | 1173 | `	};` |
|       - | 1174 | `	static const PH7_NativeMethodDef aIterator[] = {` |
|       - | 1175 | `		{ "current", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@mixed", 0 },` |
|       - | 1176 | `		{ "next",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@void", 0 },` |
|       - | 1177 | `		{ "key",     PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@mixed", 0 },` |
|       - | 1178 | `		{ "valid",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@bool", 0 },` |
|       - | 1179 | `		{ "rewind",  PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@void", 0 },` |
|       - | 1180 | `	};` |
|       - | 1181 | `	static const PH7_NativeMethodDef aIteratorAggregate[] = {` |
|       - | 1182 | `		{ "getIterator", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@Traversable", 0 },` |
|       - | 1183 | `	};` |
|       - | 1184 | `	/* php's legacy Serializable declares NO return type on either method. */` |
|       - | 1185 | `	static const PH7_NativeMethodDef aSerializable[] = {` |
|       - | 1186 | `		{ "serialize",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|       - | 1187 | `		{ "unserialize", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $data", 0, 0 },` |
|       - | 1188 | `	};` |
|       - | 1189 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1190 | `		{ "Traversable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1191 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1192 | `		{ "Stringable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1193 | `		  aStringable, SX_ARRAYSIZE(aStringable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1194 | `		{ "Throwable", "Stringable", 0, PH7_CLASS_INTERFACE,` |
|       - | 1195 | `		  aThrowable, SX_ARRAYSIZE(aThrowable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1196 | `		{ "ArrayAccess", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1197 | `		  aArrayAccess, SX_ARRAYSIZE(aArrayAccess), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1198 | `		{ "Countable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1199 | `		  aCountable, SX_ARRAYSIZE(aCountable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1200 | `		{ "JsonSerializable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1201 | `		  aJsonSerializable, SX_ARRAYSIZE(aJsonSerializable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1202 | `		{ "UnitEnum", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1203 | `		  aUnitEnum, SX_ARRAYSIZE(aUnitEnum), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1204 | `		{ "BackedEnum", "UnitEnum", 0, PH7_CLASS_INTERFACE,` |
|       - | 1205 | `		  aBackedEnum, SX_ARRAYSIZE(aBackedEnum), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1206 | `		{ "Iterator", "Traversable", 0, PH7_CLASS_INTERFACE,` |
|       - | 1207 | `		  aIterator, SX_ARRAYSIZE(aIterator), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1208 | `		{ "IteratorAggregate", "Traversable", 0, PH7_CLASS_INTERFACE,` |
|       - | 1209 | `		  aIteratorAggregate, SX_ARRAYSIZE(aIteratorAggregate), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1210 | `		{ "Serializable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1211 | `		  aSerializable, SX_ARRAYSIZE(aSerializable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1212 | `	};` |
|    5745 | 1213 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       5 | 1214 | `}` |
|       - | 1215 | `/*` |
|       - | 1216 | ` * ---------------------------------------------------------------------------` |
|       - | 1217 | `` * php's Directory — the object `dir()` answers.`` |
|       - | 1218 | ` *` |
|       - | 1219 | ` * php declares it FINAL with **no constructor at all**: the class is created by` |
|       - | 1220 | `` * `dir()` and `new Directory` is refused in the create_object handler, with a`` |
|       - | 1221 | ` * sentence that names dir() as the way to get one. Its two slots are` |
|       - | 1222 | `` * `public protected(set) readonly`, so a script can read `$d->path` and never`` |
|       - | 1223 | `` * write it, and its three methods declare return types (`read(): string\|false`).`` |
|       - | 1224 | ` * The chunk had a public constructor, a __destruct php does not declare, no` |
|       - | 1225 | ` * types anywhere and writable slots.` |
|       - | 1226 | ` * ---------------------------------------------------------------------------` |
|       - | 1227 | ` */` |
|       - | 1228 | `#define DIR_HANDLE "handle"` |
|       - | 1229 | `#define DIR_PATH   "path"` |
|       - | 1230 | `/*` |
|       - | 1231 | ` * Forward one method to the engine's own directory builtin (rule 7: call, don't` |
|       - | 1232 | `` * reimplement). php's Directory methods are `php_stream_readdir(...)` on the very`` |
|       - | 1233 | `` * stream `readdir()` uses, and a CLOSED handle is a TypeError there — the one`` |
|       - | 1234 | ` * place php's wording names the class rather than the function.` |
|       - | 1235 | ` */` |
|      30 | 1236 | `static int VmDirClosed(ph7_value *pHandle)` |
|       2 | 1237 | `{` |
|      32 | 1238 | `	io_private *pDev = (io_private *)pHandle->x.pOther;` |
|      32 | 1239 | `	return IO_PRIVATE_INVALID(pDev);` |
|       2 | 1240 | `}` |
|      30 | 1241 | `static int VmDirForward(ph7_context *pCtx,const char *zFunc,const char *zMethod)` |
|       2 | 1242 | `{` |
|      32 | 1243 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      32 | 1244 | `	ph7_value *pHandle = pThis ? PH7_NativeAttr(pThis,DIR_HANDLE) : 0;` |
|       - | 1245 | `	ph7_value *apArg[1];` |
|       - | 1246 | `	ph7_value sResult;` |
|       - | 1247 | `	ph7_value sName;` |
|       - | 1248 | `	SyString sStr;` |
|       - | 1249 | `	sxi32 rc;` |
|       - | 1250 | ``	/* php's check is `php_stream_from_zval` on a stream it CLOSED: closedir()`` |
|       - | 1251 | `	 * keeps the resource alive and marks it (gettype() answers` |
|       - | 1252 | `	 * "resource (closed)"), so the test is the magic, not the type. */` |
|      30 | 1253 | `	if( pHandle == 0 \|\| (pHandle->iFlags & MEMOBJ_RES) == 0` |
|      32 | 1254 | `	 \|\| VmDirClosed(pHandle) ){` |
|      10 | 1255 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1256 | `			"Directory::%s(): cannot use Directory resource after it has been closed",` |
|       3 | 1257 | `			zMethod);` |
|       - | 1258 | `	}` |
|      26 | 1259 | `	SyStringInitFromBuf(&sStr,zFunc,SyStrlen(zFunc));` |
|      26 | 1260 | `	PH7_MemObjInit(pCtx->pVm,&sName);` |
|      26 | 1261 | `	PH7_MemObjInitFromString(pCtx->pVm,&sName,&sStr);` |
|      26 | 1262 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      26 | 1263 | `	apArg[0] = pHandle;` |
|      26 | 1264 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,&sName,1,apArg,&sResult);` |
|      26 | 1265 | `	PH7_MemObjRelease(&sName);` |
|      26 | 1266 | `	if( rc == SXRET_OK ){` |
|      26 | 1267 | `		ph7_result_value(pCtx,&sResult);` |
|      12 | 1268 | `	}` |
|      26 | 1269 | `	PH7_MemObjRelease(&sResult);` |
|      26 | 1270 | `	return PH7_OK;` |
|      17 | 1271 | `}` |
|      20 | 1272 | `static int vm_builtin_Directory_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1273 | `{` |
|      10 | 1274 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      22 | 1275 | `	return VmDirForward(pCtx,"readdir","read");` |
|       2 | 1276 | `}` |
|       4 | 1277 | `static int vm_builtin_Directory_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1278 | `{` |
|       2 | 1279 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       5 | 1280 | `	return VmDirForward(pCtx,"rewinddir","rewind");` |
|       1 | 1281 | `}` |
|       6 | 1282 | `static int vm_builtin_Directory_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1283 | `{` |
|       3 | 1284 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       8 | 1285 | `	return VmDirForward(pCtx,"closedir","close");` |
|       2 | 1286 | `}` |
|    5740 | 1287 | `static sxi32 VmInstallDirectory(ph7_vm *pVm)` |
|       5 | 1288 | `{` |
|       - | 1289 | `	static const PH7_NativePropDef aDirProp[] = {` |
|       - | 1290 | `		{ DIR_PATH,   PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1291 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|       - | 1292 | `		{ DIR_HANDLE, PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1293 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "mixed" },` |
|       - | 1294 | `	};` |
|       - | 1295 | `	static const PH7_NativeMethodDef aDirMethod[] = {` |
|       - | 1296 | `		{ "close",  PH7_MOD_PUBLIC, "", "void", vm_builtin_Directory_close },` |
|       - | 1297 | `		{ "rewind", PH7_MOD_PUBLIC, "", "void", vm_builtin_Directory_rewind },` |
|       - | 1298 | `		{ "read",   PH7_MOD_PUBLIC, "", "string\|false", vm_builtin_Directory_read },` |
|       - | 1299 | `	};` |
|       - | 1300 | `	static const PH7_NativeClassSpec sSpec = {` |
|       - | 1301 | `		"Directory", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE,` |
|       - | 1302 | `		aDirMethod, SX_ARRAYSIZE(aDirMethod), 0, 0,` |
|       - | 1303 | `		aDirProp, SX_ARRAYSIZE(aDirProp), 0, 0, 0` |
|       - | 1304 | `	};` |
|    5745 | 1305 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|    5745 | 1306 | `	if( rc == SXRET_OK ){` |
|    5745 | 1307 | `		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"Directory",sizeof("Directory")-1,FALSE,0);` |
|    5745 | 1308 | `		if( pClass ){` |
|       - | 1309 | `			/* php words this refusal per class rather than with the generic` |
|       - | 1310 | `			 * "Instantiation of class %s is not allowed". */` |
|    5745 | 1311 | `			pClass->zNewRefusal = "Cannot directly construct Directory, use dir() instead";` |
|    2870 | 1312 | `		}` |
|    2870 | 1313 | `	}` |
|    5745 | 1314 | `	return rc;` |
|       5 | 1315 | `}` |
|       - | 1316 | `/*` |
|       - | 1317 | ` * ---------------------------------------------------------------------------` |
|       - | 1318 | ` * php's attribute classes.` |
|       - | 1319 | ` *` |
|       - | 1320 | ``  * Each carries an ATTRIBUTE of its own — `#[Attribute(Attribute::TARGET_CLASS)]` `` |
|       - | 1321 | ` * on Attribute, a target mask on every other one — and those records are` |
|       - | 1322 | ` * load-bearing rather than decorative: the engine reads them to decide whether a` |
|       - | 1323 | `` * user's `#[Deprecated]` may sit where it does, and ReflectionAttribute answers`` |
|       - | 1324 | ` * them. A compiled attribute holds its argument as byte-code, so this is what` |
|       - | 1325 | `` * `PH7_NativeClassAddAttribute()` exists for (rule 11's next unused corner,`` |
|       - | 1326 | ` * exercised here): the argument rides as a literal.` |
|       - | 1327 | ` *` |
|       - | 1328 | ` * php's Deprecated mask is 87 — TARGET_CLASS\|FUNCTION\|METHOD\|CLASS_CONSTANT\|` |
|       - | 1329 | ` * CONSTANT — where the chunk wrote 86 and left the CLASS bit out.` |
|       - | 1330 | ` *` |
|       - | 1331 | ` * Three of them declare NOTHING but their own mask, because what they mean is a` |
|       - | 1332 | `` * question something else asks: `#[AllowDynamicProperties]` is read by the`` |
|       - | 1333 | `` * dynamic-property decision at the write site, `#[SensitiveParameter]` by the`` |
|       - | 1334 | `` * backtrace builder, `#[ReturnTypeWillChange]` by php's tentative-return-type`` |
|       - | 1335 | ` * check (which the §10 non-deprecated policy removed, so nothing consults it` |
|       - | 1336 | ` * here). They still have to EXIST: a program that spells one and then asks` |
|       - | 1337 | `` * `getAttributes()[0]->newInstance()` gets php's object, not`` |
|       - | 1338 | `` * `Attribute class "AllowDynamicProperties" not found`.`` |
|       - | 1339 | ` *` |
|       - | 1340 | `` * `SensitiveParameterValue` is not an attribute at all — it is the box php puts`` |
|       - | 1341 | ` * a redacted argument in — but it belongs to the same feature and to the same` |
|       - | 1342 | ` * declaration site.` |
|       - | 1343 | ` * ---------------------------------------------------------------------------` |
|       - | 1344 | ` */` |
|       - | 1345 | ``/* php declares `public function __construct()` on the three marker attributes, so`` |
|       - | 1346 | `` * Reflection reports one and `new AllowDynamicProperties(1)` is an`` |
|       - | 1347 | ` * ArgumentCountError. The body has nothing to do: the object carries no state. */` |
|      10 | 1348 | `static int vm_builtin_AttrMarker_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1349 | `{` |
|       5 | 1350 | `	SXUNUSED(pCtx);` |
|       5 | 1351 | `	SXUNUSED(nArg);` |
|       5 | 1352 | `	SXUNUSED(apArg);` |
|      12 | 1353 | `	return PH7_OK;` |
|       2 | 1354 | `}` |
|       - | 1355 | `/* SensitiveParameterValue::__construct(mixed $value) / getValue() / __debugInfo() */` |
|       - | 1356 | `#define SPV_SLOT "value"` |
|      20 | 1357 | `static int vm_builtin_SensitiveParameterValue_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1358 | `{` |
|      21 | 1359 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      21 | 1360 | `	if( pThis && nArg > 0 ){` |
|      21 | 1361 | `		PH7_NativeSetProp(pCtx->pVm,pThis,SPV_SLOT,sizeof(SPV_SLOT)-1,apArg[0]);` |
|      10 | 1362 | `	}` |
|      21 | 1363 | `	return PH7_OK;` |
|       1 | 1364 | `}` |
|       6 | 1365 | `static int vm_builtin_SensitiveParameterValue_getValue(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1366 | `{` |
|       7 | 1367 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       7 | 1368 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,SPV_SLOT) : 0;` |
|       3 | 1369 | `	SXUNUSED(nArg);` |
|       3 | 1370 | `	SXUNUSED(apArg);` |
|       7 | 1371 | `	if( pVal ){` |
|       7 | 1372 | `		ph7_result_value(pCtx,pVal);` |
|       4 | 1373 | `	}else{` |
|     ! 0 | 1374 | `		ph7_result_null(pCtx);` |
|       - | 1375 | `	}` |
|       7 | 1376 | `	return PH7_OK;` |
|       1 | 1377 | `}` |
|       2 | 1378 | `static int vm_builtin_SensitiveParameterValue_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1379 | `{` |
|       3 | 1380 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|       1 | 1381 | `	SXUNUSED(nArg);` |
|       1 | 1382 | `	SXUNUSED(apArg);` |
|       3 | 1383 | `	if( pOut == 0 ){` |
|     ! 0 | 1384 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1385 | `	}` |
|       3 | 1386 | `	ph7_result_value(pCtx,pOut);` |
|       3 | 1387 | `	return PH7_OK;` |
|       2 | 1388 | `}` |
|       - | 1389 | `/*` |
|       - | 1390 | `` * php gives the class a `get_properties_for` handler that answers NULL for every`` |
|       - | 1391 | `` * purpose, so the box shows nothing to var_export, the `(array)` cast or`` |
|       - | 1392 | `` * json_encode either — not just to var_dump's `__debugInfo()`. The point of the`` |
|       - | 1393 | ` * class is that the value it holds does not leak onto a display surface.` |
|       - | 1394 | ` */` |
|       6 | 1395 | `static sxi32 VmPresentSensitiveParameterValue(ph7_vm *pVm,ph7_class_instance *pThis,` |
|       - | 1396 | `	ph7_value *pOut,int bDebug)` |
|       1 | 1397 | `{` |
|       3 | 1398 | `	SXUNUSED(pVm);` |
|       3 | 1399 | `	SXUNUSED(pThis);` |
|       3 | 1400 | `	SXUNUSED(pOut);` |
|       3 | 1401 | `	SXUNUSED(bDebug);` |
|       7 | 1402 | `	return SXRET_OK;   /* the empty shape, both handlers */` |
|       1 | 1403 | `}` |
|       8 | 1404 | `static int vm_builtin_Attribute_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1405 | `{` |
|       9 | 1406 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       9 | 1407 | `	if( pThis ){` |
|      16 | 1408 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,"flags",` |
|       7 | 1409 | `			nArg > 0 ? ph7_value_to_int64(apArg[0]) : 127);` |
|       4 | 1410 | `	}` |
|       9 | 1411 | `	return PH7_OK;` |
|       1 | 1412 | `}` |
|       - | 1413 | `/* NoDiscard::__construct(?string $message = null) */` |
|       2 | 1414 | `static int vm_builtin_NoDiscard_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1415 | `{` |
|       3 | 1416 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - | 1417 | `	ph7_value sVal;` |
|       3 | 1418 | `	if( pThis == 0 ){` |
|     ! 0 | 1419 | `		return PH7_OK;` |
|       - | 1420 | `	}` |
|       3 | 1421 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|       3 | 1422 | `	if( nArg > 0 ){` |
|     ! 0 | 1423 | `		PH7_MemObjStore(apArg[0],&sVal);` |
|     ! 0 | 1424 | `	}` |
|       3 | 1425 | `	PH7_NativeSetProp(pCtx->pVm,pThis,"message",sizeof("message")-1,&sVal);` |
|       3 | 1426 | `	PH7_MemObjRelease(&sVal);` |
|       3 | 1427 | `	return PH7_OK;` |
|       2 | 1428 | `}` |
|      10 | 1429 | `static int vm_builtin_Deprecated_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1430 | `{` |
|      11 | 1431 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - | 1432 | `	static const char *const azSlot[] = { "message", "since" };` |
|       - | 1433 | `	int n;` |
|      11 | 1434 | `	if( pThis == 0 ){` |
|     ! 0 | 1435 | `		return PH7_OK;` |
|       - | 1436 | `	}` |
|      31 | 1437 | `	for( n = 0 ; n < 2 ; n++ ){` |
|       - | 1438 | `		ph7_value sVal;` |
|      21 | 1439 | `		PH7_MemObjInit(pCtx->pVm,&sVal);` |
|      21 | 1440 | `		if( n < nArg ){` |
|      15 | 1441 | `			PH7_MemObjStore(apArg[n],&sVal);` |
|       7 | 1442 | `		}` |
|      21 | 1443 | `		PH7_NativeSetProp(pCtx->pVm,pThis,azSlot[n],SyStrlen(azSlot[n]),&sVal);` |
|      21 | 1444 | `		PH7_MemObjRelease(&sVal);` |
|      11 | 1445 | `	}` |
|      11 | 1446 | `	return PH7_OK;` |
|       6 | 1447 | `}` |
|    5740 | 1448 | `static sxi32 VmInstallAttributes(ph7_vm *pVm)` |
|       5 | 1449 | `{` |
|       - | 1450 | `	static const PH7_NativeConstDef aAttrConst[] = {` |
|       - | 1451 | `		{ "TARGET_CLASS",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },` |
|       - | 1452 | `		{ "TARGET_FUNCTION",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|       - | 1453 | `		{ "TARGET_METHOD",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4, 0, 0.0 },` |
|       - | 1454 | `		{ "TARGET_PROPERTY",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 8, 0, 0.0 },` |
|       - | 1455 | `		{ "TARGET_CLASS_CONSTANT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16, 0, 0.0 },` |
|       - | 1456 | `		{ "TARGET_PARAMETER",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },` |
|       - | 1457 | `		{ "TARGET_CONSTANT",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64, 0, 0.0 },` |
|       - | 1458 | `		{ "TARGET_ALL",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 127, 0, 0.0 },` |
|       - | 1459 | `		{ "IS_REPEATABLE",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 128, 0, 0.0 },` |
|       - | 1460 | `	};` |
|       - | 1461 | ``	/* php declares `public int $flags;` — typed, NO default (the constructor is`` |
|       - | 1462 | ``	 * the only writer), which is what the chunk's `public $flags;` could not say. */`` |
|       - | 1463 | `	static const PH7_NativePropDef aAttrProp[] = {` |
|       - | 1464 | `		{ "flags", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|       - | 1465 | `	};` |
|       - | 1466 | `	static const PH7_NativeMethodDef aAttrMethod[] = {` |
|       - | 1467 | `		{ "__construct", PH7_MOD_PUBLIC, "int $flags = Attribute::TARGET_ALL", 0,` |
|       - | 1468 | `		  vm_builtin_Attribute_construct },` |
|       - | 1469 | `	};` |
|       - | 1470 | `	static const PH7_NativePropDef aDepProp[] = {` |
|       - | 1471 | `		{ "message", PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1472 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "?string" },` |
|       - | 1473 | `		{ "since",   PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1474 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "?string" },` |
|       - | 1475 | `	};` |
|       - | 1476 | `	static const PH7_NativeMethodDef aDepMethod[] = {` |
|       - | 1477 | `		{ "__construct", PH7_MOD_PUBLIC, "?string $message = null, ?string $since = null", 0,` |
|       - | 1478 | `		  vm_builtin_Deprecated_construct },` |
|       - | 1479 | `	};` |
|       - | 1480 | ``	/* NoDiscard is Deprecated's shape minus the `since`. */`` |
|       - | 1481 | `	static const PH7_NativePropDef aNdProp[] = {` |
|       - | 1482 | `		{ "message", PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1483 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "?string" },` |
|       - | 1484 | `	};` |
|       - | 1485 | `	static const PH7_NativeMethodDef aNdMethod[] = {` |
|       - | 1486 | `		{ "__construct", PH7_MOD_PUBLIC, "?string $message = null", 0,` |
|       - | 1487 | `		  vm_builtin_NoDiscard_construct },` |
|       - | 1488 | `	};` |
|       - | 1489 | `	/* The three markers: one argless constructor each and no state at all. */` |
|       - | 1490 | `	static const PH7_NativeMethodDef aMarkerMethod[] = {` |
|       - | 1491 | `		{ "__construct", PH7_MOD_PUBLIC, "", 0, vm_builtin_AttrMarker_construct },` |
|       - | 1492 | `	};` |
|       - | 1493 | `	static const PH7_NativePropDef aSpvProp[] = {` |
|       - | 1494 | `		{ SPV_SLOT, PH7_MOD_PRIVATE\|PH7_MOD_READONLY,` |
|       - | 1495 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "mixed" },` |
|       - | 1496 | `	};` |
|       - | 1497 | `	static const PH7_NativeMethodDef aSpvMethod[] = {` |
|       - | 1498 | `		{ "__construct", PH7_MOD_PUBLIC, "mixed $value", 0,` |
|       - | 1499 | `		  vm_builtin_SensitiveParameterValue_construct },` |
|       - | 1500 | `		{ "getValue",    PH7_MOD_PUBLIC, "", "mixed",` |
|       - | 1501 | `		  vm_builtin_SensitiveParameterValue_getValue },` |
|       - | 1502 | `		{ "__debugInfo", PH7_MOD_PUBLIC, "", "array",` |
|       - | 1503 | `		  vm_builtin_SensitiveParameterValue_debugInfo },` |
|       - | 1504 | `	};` |
|       - | 1505 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1506 | `		{ "Attribute", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1507 | `		  aAttrMethod, SX_ARRAYSIZE(aAttrMethod), aAttrConst, SX_ARRAYSIZE(aAttrConst),` |
|       - | 1508 | `		  aAttrProp, SX_ARRAYSIZE(aAttrProp), 0, 0, 0 },` |
|       - | 1509 | `		{ "Deprecated", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1510 | `		  aDepMethod, SX_ARRAYSIZE(aDepMethod), 0, 0,` |
|       - | 1511 | `		  aDepProp, SX_ARRAYSIZE(aDepProp), 0, 0, 0 },` |
|       - | 1512 | `		{ "AllowDynamicProperties", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1513 | `		  aMarkerMethod, SX_ARRAYSIZE(aMarkerMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1514 | `		{ "SensitiveParameter", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1515 | `		  aMarkerMethod, SX_ARRAYSIZE(aMarkerMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1516 | `		{ "ReturnTypeWillChange", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1517 | `		  aMarkerMethod, SX_ARRAYSIZE(aMarkerMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1518 | `		{ "Override", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1519 | `		  aMarkerMethod, SX_ARRAYSIZE(aMarkerMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1520 | `		{ "NoDiscard", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1521 | `		  aNdMethod, SX_ARRAYSIZE(aNdMethod), 0, 0,` |
|       - | 1522 | `		  aNdProp, SX_ARRAYSIZE(aNdProp), 0, 0, 0 },` |
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
|       - | 1539 | `	static const struct {` |
|       - | 1540 | `		const char *zClass;` |
|       - | 1541 | `		const PH7_NativeAttrArg *aArg;   /* php's TARGET_* mask for that class */` |
|       - | 1542 | `	} aOwnAttr[] = {` |
|       - | 1543 | `		{ "Attribute",              aMaskClass  },   /* TARGET_CLASS */` |
|       - | 1544 | `		{ "Deprecated",             aMaskDep    },   /* CLASS\|FUNCTION\|METHOD\|CLASS_CONSTANT\|CONSTANT */` |
|       - | 1545 | `		{ "AllowDynamicProperties", aMaskClass  },   /* TARGET_CLASS */` |
|       - | 1546 | `		{ "SensitiveParameter",     aMaskParam  },   /* TARGET_PARAMETER */` |
|       - | 1547 | `		{ "ReturnTypeWillChange",   aMaskMethod },   /* TARGET_METHOD */` |
|       - | 1548 | `		{ "Override",               aMaskMembr  },   /* METHOD\|PROPERTY (php 8.5) */` |
|       - | 1549 | `		{ "NoDiscard",              aMaskCallee },   /* FUNCTION\|METHOD (php 8.5) */` |
|       - | 1550 | `	};` |
|    5745 | 1551 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       - | 1552 | `	sxu32 n;` |
|   45925 | 1553 | `	for( n = 0 ; rc == SXRET_OK && n < SX_ARRAYSIZE(aOwnAttr) ; ++n ){` |
|   60275 | 1554 | `		rc = PH7_NativeClassAddAttribute(&(*pVm),` |
|   60270 | 1555 | `			PH7_VmExtractClass(&(*pVm),aOwnAttr[n].zClass,` |
|   40180 | 1556 | `				(sxu32)SyStrlen(aOwnAttr[n].zClass),FALSE,0),` |
|   40180 | 1557 | `			"Attribute",aOwnAttr[n].aArg,1);` |
|   20095 | 1558 | `	}` |
|    5745 | 1559 | `	return rc;` |
|       5 | 1560 | `}` |
|       - | 1561 | `/*` |
|       - | 1562 | ` * stdClass and Random\RandomException.` |
|       - | 1563 | ` *` |
|       - | 1564 | ` * stdClass is EMPTY in php too — it holds only dynamic properties — so the whole` |
|       - | 1565 | `` * declaration is the row. `Random\RandomException` is the first NAMESPACED class`` |
|       - | 1566 | ` * declared from C: the engine keys its class table by the FULLY QUALIFIED name` |
|       - | 1567 | `` * (the compiler resolves `namespace Random { class RandomException }` to exactly`` |
|       - | 1568 | ` * this string before installing), so a spec row spells the FQN and needs no` |
|       - | 1569 | ` * namespace machinery at all. It also retires the chunk this file kept ALONE for` |
|       - | 1570 | `` * it, whose comment explains why: a `namespace` declaration is not reset at its`` |
|       - | 1571 | ` * closing brace here, so anything following it in the same chunk would have` |
|       - | 1572 | ` * leaked into the Random namespace.` |
|       - | 1573 | ` */` |
|    5740 | 1574 | `static sxi32 VmInstallStdClasses(ph7_vm *pVm)` |
|       5 | 1575 | `{` |
|       - | 1576 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1577 | `		{ "stdClass", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1578 | `		/* unserialize()'s carrier for a disallowed or unknown class: as empty as` |
|       - | 1579 | `		 * stdClass (its properties are the payload's, created dynamically); what` |
|       - | 1580 | `		 * makes it special is the pVm->pIncClass checks at the access sites. */` |
|       - | 1581 | `		{ "__PHP_Incomplete_Class", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1582 | `		{ "Random\\RandomException", "Exception", 0, PH7_CLASS_NOCLONE,` |
|       - | 1583 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1584 | `		/* php 8.5's filter exceptions: FILTER_THROW_ON_FAILURE raises the second,` |
|       - | 1585 | `		 * and the first is the base a caller catches to mean "any filter error". */` |
|       - | 1586 | `		{ "Filter\\FilterException", "Exception", 0, 0,` |
|       - | 1587 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1588 | `		{ "Filter\\FilterFailedException", "Filter\\FilterException", 0, 0,` |
|       - | 1589 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1590 | `	};` |
|    5745 | 1591 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       5 | 1592 | `}` |
|    5740 | 1593 | `PH7_PRIVATE sxi32 PH7_VmInstallBuiltinLib(ph7_vm *pVm)` |
|       5 | 1594 | `{` |
|       - | 1595 | `	SyString sBuiltin;` |
|       - | 1596 | `	/* The interfaces first: everything below implements one of them` |
|       - | 1597 | `	 * (Exception implements Throwable). */` |
|    5745 | 1598 | `	VmInstallCoreInterfaces(&(*pVm));` |
|    5745 | 1599 | `	VmInstallExceptions(&(*pVm));` |
|    5745 | 1600 | `	VmInstallStdClasses(&(*pVm));` |
|    5745 | 1601 | `	VmInstallDirectory(&(*pVm));` |
|    5745 | 1602 | `	VmInstallAttributes(&(*pVm));` |
|    5745 | 1603 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|       - | 1604 | `	/* Compile the built-in library */` |
|    5745 | 1605 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|    5745 | 1606 | `	return SXRET_OK;` |
|       5 | 1607 | `}` |
|       - | 1608 |  |
