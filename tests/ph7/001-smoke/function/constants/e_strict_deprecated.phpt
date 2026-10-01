--TEST--
E_STRICT is still a constant php 8.4 only deprecated, and is not part of E_ALL
--FILE--
<?php
// php 8.4 removed the error LEVEL E_STRICT but kept the CONSTANT, marked
// deprecated. This engine had dropped the name outright, so `E_STRICT` was an
// uncatchable "Undefined constant" Error where php hands back 2048 and merely
// says so -- a script-visible difference, not an ini one.
$estrSeen = [];
set_error_handler(function ($n, $s) use (&$estrSeen) { $estrSeen[] = "$n|$s"; return true; });

// defined() is a SILENT fetch: it answers without raising.
$estrBefore = count($estrSeen);
echo 'defined=', var_export(defined('E_STRICT'), true),
     ' raised=', count($estrSeen) - $estrBefore, "\n";

// Naming it does raise, at the engine's E_DEPRECATED (8192), not the userland
// E_USER_DEPRECATED an attribute-authored one would take.
$estrValue = E_STRICT;
echo 'value=', $estrValue, ' ', $estrSeen[count($estrSeen) - 1], "\n";

// constant() is the same read, and every access re-warns.
$estrBefore = count($estrSeen);
constant('E_STRICT');
constant('E_STRICT');
echo 'repeats=', count($estrSeen) - $estrBefore, "\n";

// LISTING the table is not naming one.
$estrBefore = count($estrSeen);
$estrAll = get_defined_constants(true);
echo 'listing=', count($estrSeen) - $estrBefore,
     ' core=', var_export($estrAll['Core']['E_STRICT'] ?? null, true), "\n";

// The level is gone from E_ALL: 30719, not 32767.
echo 'E_ALL=', E_ALL, ' contains=', var_export((bool)(E_ALL & 2048), true), "\n";

// It still round-trips through error_reporting() as a plain bit.
$estrOld = error_reporting(2048);
echo 'reporting=', error_reporting(), "\n";
error_reporting($estrOld);

// Reflection reads the same fact the notice does.
$estrRef = new ReflectionConstant('E_STRICT');
echo 'deprecated=', var_export($estrRef->isDeprecated(), true), "\n";
echo $estrRef, "\n";
restore_error_handler();
--EXPECT--
defined=true raised=0
value=2048 8192|Constant E_STRICT is deprecated since 8.4, the error level was removed
repeats=2
listing=0 core=2048
E_ALL=30719 contains=false
reporting=2048
deprecated=true
Constant [ <persistent, deprecated> int E_STRICT ] { 2048 }
