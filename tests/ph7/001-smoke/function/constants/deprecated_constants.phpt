--TEST--
A constant php deprecated the SYMBOL of says so when a program names it
--FILE--
<?php
// This engine's constants that php 8.x deprecated the SYMBOL of. Most were once
// SILENT: only MT_RAND_PHP announced itself, from a raise hand-written into its
// own expander. They carry the mark now and the notice is raised where every
// constant's is, so each says what php says.
$depcSeen = [];
set_error_handler(function ($n, $s) use (&$depcSeen) { $depcSeen[] = "$n|$s"; return true; });
foreach (['DATE_RFC7231', 'MT_RAND_PHP', 'FILE_TEXT', 'FILE_BINARY',
          'DOM_PHP_ERR', 'E_STRICT', 'PHP_EOL', 'SORT_STRING', 'M_PI'] as $depcName) {
    $depcBefore = count($depcSeen);
    if (!defined($depcName)) { echo $depcName, " ABSENT\n"; continue; }
    constant($depcName);
    echo $depcName, ' ', count($depcSeen) > $depcBefore ? $depcSeen[count($depcSeen) - 1] : '(silent)', "\n";
}
// Every access re-warns, the way php's does.
$depcBefore = count($depcSeen);
constant('FILE_TEXT');
constant('FILE_TEXT');
echo 'repeats=', count($depcSeen) - $depcBefore, "\n";

// Naming it in SOURCE is the same read as constant().
$depcBefore = count($depcSeen);
$depcValue = FILE_TEXT;
echo 'literal=', count($depcSeen) - $depcBefore, ' value=', $depcValue, "\n";

// LISTING the table is not naming one, so the description is silent.
$depcBefore = count($depcSeen);
$depcAll = get_defined_constants();
echo 'listing=', count($depcSeen) - $depcBefore, ' has=', var_export(isset($depcAll['FILE_TEXT']), true), "\n";
restore_error_handler();
--EXPECT--
DATE_RFC7231 8192|Constant DATE_RFC7231 is deprecated since 8.5, as this format ignores the associated timezone and always uses GMT
MT_RAND_PHP 8192|Constant MT_RAND_PHP is deprecated since 8.3, as it uses a biased non-standard variant of Mt19937
FILE_TEXT 8192|Constant FILE_TEXT is deprecated since 8.1, as the constant has no effect
FILE_BINARY 8192|Constant FILE_BINARY is deprecated since 8.1, as the constant has no effect
DOM_PHP_ERR 8192|Constant DOM_PHP_ERR is deprecated since 8.4, as it is no longer used
E_STRICT 8192|Constant E_STRICT is deprecated since 8.4, the error level was removed
PHP_EOL (silent)
SORT_STRING (silent)
M_PI (silent)
repeats=2
literal=1 value=0
listing=0 has=true
