--TEST--
array_replace_recursive() refuses a named argument it has no parameter for
--FILE--
<?php
// array_replace_recursive() is an internal variadic: a name it has no parameter
// for -- its own variadic's name included -- is refused from inside the call,
// after the arity screen and before the type screens, on every call door.
function narv_run($label, $f, $trace = false) {
    try {
        echo $label, ': ';
        var_dump($f());
    } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
        if ($trace) {
            echo explode("\n", $e->getTraceAsString())[0], "\n";
        }
    }
}
function narv_g() {
    narv_run('extra', fn() => array_replace_recursive([1], zz: [2]), true);
    narv_run('extra after replacement', fn() => array_replace_recursive([1], [2], zz: 1));
    narv_run('variadic name', fn() => array_replace_recursive(array: [1], replacements: [2]));
    narv_run('before types', fn() => array_replace_recursive('x', zz: [1]), true);
    narv_run('after arity', fn() => array_replace_recursive(replacements: [1]));
    narv_run('named first', fn() => array_replace_recursive(zz: [1], array: [1]));
    narv_run('unpacked', fn() => array_replace_recursive([1], ...[[2], 'q' => [3]]), true);
    narv_run('call_user_func', fn() => call_user_func('array_replace_recursive', [1], zz: [2]));
    narv_run('call_user_func_array', fn() => call_user_func_array('array_replace_recursive', [[1], 'zz' => [2]]));
    narv_run('invokeArgs', fn() => (new ReflectionFunction('array_replace_recursive'))->invokeArgs([[1], 'zz' => [2]]));
    narv_run('first-class', fn() => array_replace_recursive(...)([1], zz: [2]));
    narv_run('declared name', fn() => array_replace_recursive(array: [1, [2]]));
    narv_run('overwrite', fn() => array_replace_recursive([1], [2], array: [3]));
}
narv_g();
--EXPECTF--
extra: ArgumentCountError: array_replace_recursive() does not accept unknown named parameters
#0 %s(17): array_replace_recursive()
extra after replacement: ArgumentCountError: array_replace_recursive() does not accept unknown named parameters
variadic name: ArgumentCountError: array_replace_recursive() does not accept unknown named parameters
before types: ArgumentCountError: array_replace_recursive() does not accept unknown named parameters
#0 %s(20): array_replace_recursive()
after arity: ArgumentCountError: array_replace_recursive() expects at least 1 argument, 0 given
named first: ArgumentCountError: array_replace_recursive() does not accept unknown named parameters
unpacked: ArgumentCountError: array_replace_recursive() does not accept unknown named parameters
#0 %s(23): array_replace_recursive()
call_user_func: ArgumentCountError: array_replace_recursive() does not accept unknown named parameters
call_user_func_array: ArgumentCountError: array_replace_recursive() does not accept unknown named parameters
invokeArgs: ArgumentCountError: array_replace_recursive() does not accept unknown named parameters
first-class: ArgumentCountError: array_replace_recursive() does not accept unknown named parameters
declared name: array(2) {
  [0]=>
  int(1)
  [1]=>
  array(1) {
    [0]=>
    int(2)
  }
}
overwrite: Error: Named parameter $array overwrites previous argument
