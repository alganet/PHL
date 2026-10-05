--TEST--
An internal function's trace frame lists its arguments
--FILE--
<?php
// An internal function's trace frame lists the arguments it was called with,
// the way a user frame does: array_map's above its callback, usort's by-reference
// array as it stands when the callback runs, a native method's, the Fiber method
// that entered a fiber's body last, and a Generator method resuming its body.
// The key is there even when nothing was passed, and IGNORE_ARGS drops it again.
function show() {
    foreach (debug_backtrace() as $fr) {
        if ($fr['function'] === 'show') continue;
        echo '  ', isset($fr['file']) ? 'line ' . $fr['line'] : '[internal]', ' ',
            isset($fr['class']) ? $fr['class'] . $fr['type'] : '',
            str_replace(__FILE__, 'FILE', $fr['function']), ' ',
            array_key_exists('args', $fr) ? json_encode($fr['args']) : 'no args', "\n";
    }
    echo "==\n";
}
function f(...$a) { show(); return 1; }
array_map('f', [1, 2]);
array_map('call_user_func', ['f']);
array_filter(['k' => 4], 'f', ARRAY_FILTER_USE_BOTH);
$a = [3, 1];
usort($a, function ($x, $y) { show(); return $x <=> $y; });
array_walk($a, function (&$v, $k, $u) { $v *= 10; show(); }, 'extra');
preg_replace_callback('/b/', function ($m) { show(); return ''; }, 'abc');
(new ReflectionFunction('f'))->invoke(7, 8);
$fb = new Fiber(function ($x) { show(); Fiber::suspend(); show(); });
$fb->start('s');
$fb->resume('r');
function gen() { yield 1; show(); yield 2; }
$g = gen();
$g->current();
$g->next();
$g = gen();
$g->current();
$g->send('v');
array_map(function ($x) {
    $fr = debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS)[1];
    echo $fr['function'], ' ', array_key_exists('args', $fr) ? 'args' : 'no args', "\n";
}, [1]);
array_map(function ($x) {
    ob_start();
    debug_print_backtrace();
    echo str_replace(__FILE__, 'FILE', ob_get_clean());
}, ['p']);
--EXPECT--
  [internal] f [1]
  line 18 array_map ["f",[1,2]]
==
  [internal] f [2]
  line 18 array_map ["f",[1,2]]
==
  [internal] f []
  [internal] call_user_func ["f"]
  line 19 array_map ["call_user_func",["f"]]
==
  [internal] f [4,"k"]
  line 20 array_filter [{"k":4},"f",1]
==
  [internal] {closure:FILE:22} [3,1]
  line 22 usort [[3,1],{}]
==
  [internal] {closure:FILE:23} [10,0,"extra"]
  line 23 array_walk [[10,3],{},"extra"]
==
  [internal] {closure:FILE:23} [30,1,"extra"]
  line 23 array_walk [[10,30],{},"extra"]
==
  [internal] {closure:FILE:24} [["b"]]
  line 24 preg_replace_callback ["\/b\/",{},"abc"]
==
  [internal] f [7,8]
  line 25 ReflectionFunction->invoke [7,8]
==
  [internal] {closure:FILE:26} ["s"]
  line 27 Fiber->start ["s"]
==
  [internal] {closure:FILE:26} ["s"]
  line 28 Fiber->resume ["r"]
==
  [internal] gen []
  line 32 Generator->next []
==
  [internal] gen []
  line 35 Generator->send ["v"]
==
array_map no args
#0 [internal function]: {closure:FILE:40}('...')
#1 FILE(40): array_map(Object(Closure), Array)
