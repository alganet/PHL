--TEST--
A variadic builtin binds a declared parameter by name and collects an unknown name as an extra, which only the forwards take
--FILE--
<?php
class BvnaHost { public $v = 7; }
function bvna_rest(...$a) { return $a; }
function bvna_pair($p, $q = 5) { return [$p, $q]; }
function bvna_try(string $label, callable $f) {
    try {
        $r = json_encode($f());
    } catch (Throwable $e) {
        $t = $e->getTrace();
        $r = get_class($e) . ': ' . $e->getMessage() . ' [' . (str_starts_with($t[0]['function'] ?? '-', '{closure') ? 'caller' : ($t[0]['function'] ?? '-')) . ']';
    }
    echo $label, ' => ', $r, "\n";
}
$o = new BvnaHost;
$add = function ($x) { return $this->v + $x; };
$rest = function (...$a) { return $a; };

echo "-- a declared parameter of a variadic signature binds by name\n";
bvna_try('call(x:, newThis:)', fn() => $add->call(x: 3, newThis: $o));
bvna_try('call(newThis:, x:)', fn() => $add->call(newThis: $o, x: 4));
bvna_try('call(3, newThis:)', fn() => $add->call(3, newThis: $o));
bvna_try('call(x:)', fn() => $add->call(x: 2));
bvna_try('max(1, 2, value:)', fn() => max(1, 2, value: 3));
bvna_try('max(value:)', fn() => max(value: [1, 9, 2]));
bvna_try('sprintf(format:)', fn() => sprintf(format: 'ok'));
bvna_try('array_map(callback:, array:)', fn() => array_map(array: [1, 2], callback: fn($v) => $v * 2));

echo "-- an unknown name is an extra, keyed, for the forwards\n";
bvna_try('call(x:, newThis:) rest', fn() => $rest->call(x: 2, newThis: $o));
bvna_try('call(args:, newThis:) rest', fn() => $rest->call(args: 2, newThis: $o));
bvna_try('call($o, 1, x:) rest', fn() => $rest->call($o, 1, x: 2));
bvna_try('call_user_func(x:, callback:)', fn() => call_user_func(x: 1, callback: 'bvna_rest'));
bvna_try('call_user_func(callback:, args:)', fn() => call_user_func(callback: 'bvna_rest', args: 1));
bvna_try('call_user_func(callback:, q:, p:)', fn() => call_user_func(callback: 'bvna_pair', q: 1, p: 2));
bvna_try('call_user_func(x:)', fn() => call_user_func(x: 1));

echo "-- every other variadic builtin refuses it, after its declared screens\n";
bvna_try('sprintf(zz:)', fn() => sprintf(zz: 1));
bvna_try('sprintf(format:, zz:)', fn() => sprintf(format: '%d', zz: 1));
bvna_try('sprintf(format: [], zz:)', fn() => sprintf(format: [], zz: 1));
bvna_try('sprintf(values:, format:)', fn() => sprintf(values: 1, format: '%d'));
bvna_try('sprintf(fmt, 1, 2, zz:)', fn() => sprintf('%d-%d', 1, 2, zz: 1));
bvna_try('printf(format:, zz:)', fn() => printf(format: "%s\n", zz: 1));
bvna_try('max(zz:)', fn() => max(zz: 1));
bvna_try('max(value:, values:)', fn() => max(value: [1, 2], values: 3));
bvna_try('pack(format:, values:)', fn() => pack(format: 'C', values: 65));
bvna_try('var_dump(value:, zz:)', fn() => var_dump(value: 1, zz: 2));
bvna_try('array_map(null, [1], zz:)', fn() => array_map(null, [1], zz: [2]));
bvna_try('forward_static_call(f, zz:)', fn() => forward_static_call('bvna_rest', zz: 1));
bvna_try('array_merge(...assoc)', fn() => array_merge(...['a' => [1], 'b' => [2]]));
bvna_try('array_merge(...mixed)', fn() => array_merge(...[[1], 'b' => [2]]));
bvna_try('array_push([], zz:)', function () { $a = []; return array_push($a, zz: 1); });

echo "-- ...and some parse the extras before the arity or the types\n";
bvna_try('array_intersect(zz:)', fn() => array_intersect(...['zz' => 1]));
bvna_try('array_udiff(zz:)', fn() => array_udiff(...['zz' => 1]));
bvna_try('array_diff_ukey(1, zz:)', fn() => array_diff_ukey(...[1, 'zz' => 1]));
bvna_try('register_shutdown_function(zz:)', fn() => register_shutdown_function(...['zz' => 1]));
bvna_try('array_diff(zz:)', fn() => array_diff(...['zz' => 1]));
bvna_try('array_diff(1, zz:)', fn() => array_diff(...[1, 'zz' => 1]));
bvna_try('array_replace(1, zz:)', fn() => array_replace(...[1, 'zz' => 1]));
bvna_try('array_push(1, zz:) typed', function () { $a = 1; return array_push($a, zz: 1); });
bvna_try('sprintf([], zz:) typed', fn() => sprintf(...[[], 'zz' => 1]));
--EXPECT--
-- a declared parameter of a variadic signature binds by name
call(x:, newThis:) => 10
call(newThis:, x:) => 11
call(3, newThis:) => Error: Named parameter $newThis overwrites previous argument [caller]
call(x:) => ArgumentCountError: Closure::call() expects at least 1 argument, 0 given [call]
max(1, 2, value:) => Error: Named parameter $value overwrites previous argument [caller]
max(value:) => 9
sprintf(format:) => "ok"
array_map(callback:, array:) => [2,4]
-- an unknown name is an extra, keyed, for the forwards
call(x:, newThis:) rest => {"x":2}
call(args:, newThis:) rest => {"args":2}
call($o, 1, x:) rest => {"0":1,"x":2}
call_user_func(x:, callback:) => {"x":1}
call_user_func(callback:, args:) => {"args":1}
call_user_func(callback:, q:, p:) => [2,1]
call_user_func(x:) => ArgumentCountError: call_user_func() expects at least 1 argument, 0 given [call_user_func]
-- every other variadic builtin refuses it, after its declared screens
sprintf(zz:) => ArgumentCountError: sprintf() expects at least 1 argument, 0 given [sprintf]
sprintf(format:, zz:) => ArgumentCountError: sprintf() does not accept unknown named parameters [sprintf]
sprintf(format: [], zz:) => TypeError: sprintf(): Argument #1 ($format) must be of type string, array given [sprintf]
sprintf(values:, format:) => ArgumentCountError: sprintf() does not accept unknown named parameters [sprintf]
sprintf(fmt, 1, 2, zz:) => ArgumentCountError: sprintf() does not accept unknown named parameters [sprintf]
printf(format:, zz:) => ArgumentCountError: printf() does not accept unknown named parameters [printf]
max(zz:) => ArgumentCountError: max() expects at least 1 argument, 0 given [max]
max(value:, values:) => ArgumentCountError: max() does not accept unknown named parameters [max]
pack(format:, values:) => ArgumentCountError: pack() does not accept unknown named parameters [pack]
var_dump(value:, zz:) => ArgumentCountError: var_dump() does not accept unknown named parameters [var_dump]
array_map(null, [1], zz:) => ArgumentCountError: array_map() does not accept unknown named parameters [array_map]
forward_static_call(f, zz:) => ArgumentCountError: forward_static_call() does not accept unknown named parameters [forward_static_call]
array_merge(...assoc) => ArgumentCountError: array_merge() does not accept unknown named parameters [array_merge]
array_merge(...mixed) => ArgumentCountError: array_merge() does not accept unknown named parameters [array_merge]
array_push([], zz:) => ArgumentCountError: array_push() does not accept unknown named parameters [array_push]
-- ...and some parse the extras before the arity or the types
array_intersect(zz:) => ArgumentCountError: array_intersect() does not accept unknown named parameters [array_intersect]
array_udiff(zz:) => ArgumentCountError: array_udiff() does not accept unknown named parameters [array_udiff]
array_diff_ukey(1, zz:) => ArgumentCountError: array_diff_ukey() does not accept unknown named parameters [array_diff_ukey]
register_shutdown_function(zz:) => ArgumentCountError: register_shutdown_function() does not accept unknown named parameters [register_shutdown_function]
array_diff(zz:) => ArgumentCountError: array_diff() expects at least 1 argument, 0 given [array_diff]
array_diff(1, zz:) => ArgumentCountError: array_diff() does not accept unknown named parameters [array_diff]
array_replace(1, zz:) => ArgumentCountError: array_replace() does not accept unknown named parameters [array_replace]
array_push(1, zz:) typed => TypeError: array_push(): Argument #1 ($array) must be of type array, int given [array_push]
sprintf([], zz:) typed => TypeError: sprintf(): Argument #1 ($format) must be of type string, array given [sprintf]
