--TEST--
A call_user_func php's compiler does not fold is a real internal call
--FILE--
<?php
declare(strict_types=1);
namespace X;
// php folds call_user_func()/call_user_func_array() into a direct call only for
// a compile-time bound spelling: global or qualified name, no spread, and no
// `name:` argument. forward_static_call() and _array() are never folded. Every
// other shape is a real internal call: the forward keeps its own trace frame,
// the callback's frame has no call site (and its refusal names none), the
// callback binds weakly even from a strict file, runs as a dynamic call, and a
// dropped answer belongs to the forward, not to a #[\NoDiscard] callback.
function f($a, $b) {}
function t(int $a) { return $a; }
#[\NoDiscard] function nd($a = 0) { return 1; }
function bt($a = 0, $b = 0) {
    foreach (debug_backtrace() as $fr) {
        echo '  ', isset($fr['file']) ? 'line ' . $fr['line'] : '[internal]', ' ', str_replace(__FILE__, 'FILE', $fr['function']), "\n";
    }
}
class A {
    static function fsc() { return \forward_static_call('X\f', 1); }
    static function fsca() { return \forward_static_call_array('X\t', ['5']); }
}
function show(string $label, callable $c) {
    echo "$label:\n";
    try {
        $r = $c();
        if ($r !== null) { echo '  = ', json_encode($r), "\n"; }
    } catch (\Throwable $e) {
        echo '  ', get_class($e), ': ', str_replace(__FILE__, 'FILE', $e->getMessage()), "\n";
        foreach ($e->getTrace() as $fr) {
            echo '  #', isset($fr['file']) ? 'line ' . $fr['line'] : '[internal]', ' ', str_replace(__FILE__, 'FILE', $fr['function']), "\n";
            if ($fr['function'] === 'show') break;
        }
    }
}
show('folded too-few', fn() => \call_user_func('X\f', 1));
show('folded strict', fn() => \call_user_func('X\t', '5'));
show('named hole', fn() => \call_user_func('X\f', b: 1));
show('named strict', fn() => \call_user_func('X\t', a: '5'));
show('named callback', fn() => \call_user_func(callback: 'X\bt'));
show('named cufa', fn() => \call_user_func_array(args: ['b' => 1], callback: 'X\f'));
show('named cufa strict', fn() => \call_user_func_array(args: ['5'], callback: 'X\t'));
show('array keys strict', fn() => \call_user_func_array('X\t', ['a' => '5']));
show('unqualified strict', fn() => call_user_func('X\t', '5'));
show('unqualified too-few', fn() => call_user_func('X\f', 1));
show('spread strict', fn() => \call_user_func_array(...['X\t', ['5']]));
show('spread too-few', fn() => \call_user_func_array(...['X\f', [1]]));
show('fsc too-few', fn() => A::fsc());
show('fsca strict', fn() => A::fsca());
show('named compact', function () { $a = 1; return \call_user_func('compact', var_name: 'a'); });
show('named cufa compact', function () { $a = 1; return \call_user_func_array(args: ['a'], callback: 'compact'); });
show('folded compact', function () { $a = 1; return \call_user_func('compact', 'a'); });
set_error_handler(function ($n, $m) { echo '  ', $m, "\n"; return true; });
echo "discard:\n";
\call_user_func('X\nd', a: 1);
\call_user_func_array(args: [1], callback: 'X\nd');
echo "folded discard:\n";
\call_user_func('X\nd', 1);
--EXPECT--
folded too-few:
  ArgumentCountError: Too few arguments to function X\f(), 1 passed in FILE on line 36 and exactly 2 expected
  #line 36 X\f
  #line 26 {closure:FILE:36}
  #line 36 X\show
folded strict:
  TypeError: X\t(): Argument #1 ($a) must be of type int, string given, called in FILE on line 37
  #line 37 X\t
  #line 26 {closure:FILE:37}
  #line 37 X\show
named hole:
  ArgumentCountError: X\f(): Argument #1 ($a) not passed
  #[internal] X\f
  #line 38 call_user_func
  #line 26 {closure:FILE:38}
  #line 38 X\show
named strict:
  = 5
named callback:
  [internal] X\bt
  line 40 call_user_func
  line 26 {closure:FILE:40}
  line 40 X\show
named cufa:
  ArgumentCountError: X\f(): Argument #1 ($a) not passed
  #[internal] X\f
  #line 41 call_user_func_array
  #line 26 {closure:FILE:41}
  #line 41 X\show
named cufa strict:
  = 5
array keys strict:
  TypeError: X\t(): Argument #1 ($a) must be of type int, string given, called in FILE on line 43
  #line 43 X\t
  #line 26 {closure:FILE:43}
  #line 43 X\show
unqualified strict:
  = 5
unqualified too-few:
  ArgumentCountError: Too few arguments to function X\f(), 1 passed and exactly 2 expected
  #[internal] X\f
  #line 45 call_user_func
  #line 26 {closure:FILE:45}
  #line 45 X\show
spread strict:
  = 5
spread too-few:
  ArgumentCountError: Too few arguments to function X\f(), 1 passed and exactly 2 expected
  #[internal] X\f
  #line 47 call_user_func_array
  #line 26 {closure:FILE:47}
  #line 47 X\show
fsc too-few:
  ArgumentCountError: Too few arguments to function X\f(), 1 passed and exactly 2 expected
  #[internal] X\f
  #line 20 forward_static_call
  #line 48 fsc
  #line 26 {closure:FILE:48}
  #line 48 X\show
fsca strict:
  = 5
named compact:
  Error: Cannot call compact() dynamically
  #[internal] compact
  #line 50 call_user_func
  #line 26 {closure:FILE:50}
  #line 50 X\show
named cufa compact:
  Error: Cannot call compact() dynamically
  #[internal] compact
  #line 51 call_user_func_array
  #line 26 {closure:FILE:51}
  #line 51 X\show
folded compact:
  = {"a":1}
discard:
folded discard:
  The return value of function X\nd() should either be used or intentionally ignored by casting it as (void)
