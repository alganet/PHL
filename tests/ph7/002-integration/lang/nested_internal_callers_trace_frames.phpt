--TEST--
A callback reached through nested internal calls has a frame for each
--FILE--
<?php
// A callback reached through MORE than one internal call gets a trace frame for
// each: array_map('call_user_func', ...) runs the callback from inside
// call_user_func, run from inside array_map, and only the outermost carries the
// userland line. A call_user_func php's compiler folds is no call at all, so it
// has no frame -- neither above what its callback runs nor under a refusal of
// its own callback screen. A native method is named by its declaring class with
// php's `->`, however it was reached.
function f(int $x) { throw new Exception('f'); }
function g() {
    foreach (debug_backtrace() as $fr) {
        echo '  ', isset($fr['file']) ? 'line ' . $fr['line'] : '[internal]', ' ',
            isset($fr['class']) ? $fr['class'] . $fr['type'] : '', str_replace(__FILE__, 'FILE', $fr['function']), "\n";
    }
}
class R extends ReflectionFunction {}
function show(string $label, callable $c) {
    echo "$label:\n";
    try {
        $c();
    } catch (Throwable $e) {
        echo '  ', get_class($e), ': ', $e->getMessage(), "\n";
        foreach ($e->getTrace() as $fr) {
            if ($fr['function'] === 'show') break;
            echo '  #', isset($fr['file']) ? 'line ' . $fr['line'] : '[internal]', ' ',
                isset($fr['class']) ? $fr['class'] . $fr['type'] : '', str_replace(__FILE__, 'FILE', $fr['function']), "\n";
        }
    }
}
show('array_map > call_user_func', fn() => array_map('call_user_func', ['f'], [1]));
show('array_map > call_user_func_array', fn() => array_map('call_user_func_array', ['f'], [[1]]));
show('array_map > array_map', fn() => array_map('array_map', ['f'], [[1]]));
show('invoke > call_user_func', fn() => (new ReflectionFunction('call_user_func'))->invoke('f', 1));
show('invoke', fn() => (new ReflectionFunction('f'))->invoke(1));
show('subclass invoke', fn() => (new R('f'))->invoke(1));
show('folded > array_map', fn() => call_user_func('array_map', 'f', [1]));
show('folded > folded > f', fn() => call_user_func('call_user_func', 'f', 1));
show('folded > array_map > builtin', fn() => call_user_func('array_map', 'str_repeat', ['x'], [-1]));
show('folded screen', fn() => call_user_func('nope'));
show('folded array screen', fn() => call_user_func_array('nope', []));
show('unfolded screen', fn() => call_user_func('nope', a: 1));
echo "debug_backtrace, array_map > call_user_func:\n";
array_map('call_user_func', ['g']);
echo "debug_backtrace, folded > array_map:\n";
call_user_func('array_map', 'g', [1]);
--EXPECT--
array_map > call_user_func:
  Exception: f
  #[internal] f
  #[internal] call_user_func
  #line 30 array_map
  #line 20 {closure:FILE:30}
array_map > call_user_func_array:
  Exception: f
  #[internal] f
  #[internal] call_user_func_array
  #line 31 array_map
  #line 20 {closure:FILE:31}
array_map > array_map:
  Exception: f
  #[internal] f
  #[internal] array_map
  #line 32 array_map
  #line 20 {closure:FILE:32}
invoke > call_user_func:
  Exception: f
  #[internal] f
  #[internal] call_user_func
  #line 33 ReflectionFunction->invoke
  #line 20 {closure:FILE:33}
invoke:
  Exception: f
  #[internal] f
  #line 34 ReflectionFunction->invoke
  #line 20 {closure:FILE:34}
subclass invoke:
  Exception: f
  #[internal] f
  #line 35 ReflectionFunction->invoke
  #line 20 {closure:FILE:35}
folded > array_map:
  Exception: f
  #[internal] f
  #line 36 array_map
  #line 20 {closure:FILE:36}
folded > folded > f:
  Exception: f
  #[internal] f
  #line 37 call_user_func
  #line 20 {closure:FILE:37}
folded > array_map > builtin:
  ValueError: str_repeat(): Argument #2 ($times) must be greater than or equal to 0
  #[internal] str_repeat
  #line 38 array_map
  #line 20 {closure:FILE:38}
folded screen:
  TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, function "nope" not found or invalid function name
  #line 20 {closure:FILE:39}
folded array screen:
  TypeError: call_user_func_array(): Argument #1 ($callback) must be a valid callback, function "nope" not found or invalid function name
  #line 20 {closure:FILE:40}
unfolded screen:
  TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, function "nope" not found or invalid function name
  #line 41 call_user_func
  #line 20 {closure:FILE:41}
debug_backtrace, array_map > call_user_func:
  [internal] g
  [internal] call_user_func
  line 43 array_map
debug_backtrace, folded > array_map:
  [internal] g
  line 45 array_map
