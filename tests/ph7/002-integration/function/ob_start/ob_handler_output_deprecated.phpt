--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A user output handler that prints is deprecated once per run, under the function that ran it
--DESCRIPTION--
php 8.4 discards what a handler prints and raises "Producing output from user output
handler NAME is deprecated" after every call that printed, from whichever door ran it:
each ob_* member that invokes the handler, the echo (or builtin, or callback) whose
write filled a chunked buffer, one per level when buffers nest. A handler that throws
is not deprecated, and neither is one that prints an empty string. The messages are
collected through a handler: the box's php.ini masks E_DEPRECATED from the display.
--FILE--
<?php
$log = [];
set_error_handler(function ($n, $m) use (&$log) { $log[] = "$n: $m"; return true; });
function h($buf, $phase) { echo "X"; return "[$buf|$phase]"; }
function quiet($buf) { echo ""; return "[$buf]"; }
function thrower($buf) { echo "X"; throw new Exception("t"); }
function g($v) { echo "abc"; return $v; }
function f() { echo "abc"; }
class C {
    function m($b) { echo "y"; return $b; }
    static function s($b) { print "z"; return $b; }
}
foreach (['ob_flush', 'ob_get_flush', 'ob_clean', 'ob_end_clean', 'ob_get_clean', 'ob_end_flush', 'ob_get_contents'] as $fn) {
    ob_start('h');
    echo "a";
    $r = $fn();
    if (!in_array($fn, ['ob_end_clean', 'ob_get_clean', 'ob_end_flush', 'ob_get_flush'])) {
        ob_end_clean();
    }
    $log[] = "-- $fn -> " . var_export($r, true);
}
ob_start('h'); ob_start('h'); echo "b"; ob_end_clean(); ob_end_clean();
$log[] = "-- nested";
ob_start([new C, 'm']); echo "c"; ob_end_flush(); $log[] = "-- method";
ob_start('C::s'); echo "c"; ob_end_flush(); $log[] = "-- static";
ob_start(function ($b) { echo "q"; return $b; }); echo "c"; ob_end_flush(); $log[] = "-- closure";
ob_start('var_dump'); echo "v"; ob_end_flush(); $log[] = "-- builtin handler";
ob_start('quiet'); echo "c"; ob_end_flush(); $log[] = "-- empty echo";
ob_start('h', 2); echo "abcdef"; ob_end_clean(); $log[] = "-- chunk, top level";
ob_start('h', 2); f(); ob_end_clean(); $log[] = "-- chunk, in a function";
ob_start('h', 2); printf("%s", "abc"); ob_end_clean(); $log[] = "-- chunk, in a builtin";
ob_start('h', 2); array_map('g', [1]); ob_end_clean(); $log[] = "-- chunk, in a builtin's callback";
try { ob_start('thrower'); echo "a"; ob_end_clean(); } catch (Exception $e) { $log[] = "caught"; }
$log[] = "-- throwing handler";
restore_error_handler();
echo "\n", implode("\n", $log), "\n";
--EXPECTF--
[a|5][a|9][a|9]ccc[c][abcdef|1][abc|1][abc|1][abc|1]
8192: ob_flush(): Producing output from user output handler h is deprecated
8192: ob_end_clean(): Producing output from user output handler h is deprecated
-- ob_flush -> true
8192: ob_get_flush(): Producing output from user output handler h is deprecated
-- ob_get_flush -> 'a'
8192: ob_clean(): Producing output from user output handler h is deprecated
8192: ob_end_clean(): Producing output from user output handler h is deprecated
-- ob_clean -> true
8192: ob_end_clean(): Producing output from user output handler h is deprecated
-- ob_end_clean -> true
8192: ob_get_clean(): Producing output from user output handler h is deprecated
-- ob_get_clean -> 'a'
8192: ob_end_flush(): Producing output from user output handler h is deprecated
-- ob_end_flush -> true
8192: ob_end_clean(): Producing output from user output handler h is deprecated
-- ob_get_contents -> 'a'
8192: ob_end_clean(): Producing output from user output handler h is deprecated
8192: ob_end_clean(): Producing output from user output handler h is deprecated
-- nested
8192: ob_end_flush(): Producing output from user output handler C::m is deprecated
-- method
8192: ob_end_flush(): Producing output from user output handler C::s is deprecated
-- static
8192: ob_end_flush(): Producing output from user output handler {closure:%s:26} is deprecated
-- closure
8192: ob_end_flush(): Producing output from user output handler var_dump is deprecated
-- builtin handler
-- empty echo
8192: main(): Producing output from user output handler h is deprecated
8192: ob_end_clean(): Producing output from user output handler h is deprecated
-- chunk, top level
8192: f(): Producing output from user output handler h is deprecated
8192: ob_end_clean(): Producing output from user output handler h is deprecated
-- chunk, in a function
8192: printf(): Producing output from user output handler h is deprecated
8192: ob_end_clean(): Producing output from user output handler h is deprecated
-- chunk, in a builtin
8192: g(): Producing output from user output handler h is deprecated
8192: ob_end_clean(): Producing output from user output handler h is deprecated
-- chunk, in a builtin's callback
caught
-- throwing handler
