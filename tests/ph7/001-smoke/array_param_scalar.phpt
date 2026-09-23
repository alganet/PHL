--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A scalar cannot satisfy a parameter typed exactly array
--DESCRIPTION--
No coercion produces an array, so php refuses a scalar handed to an `array`
parameter outright. The shared signature screen exempted every type list
carrying an `array` arm for a wording reason that only applies to a UNION:
php's `array|object` parameters come from one ZPP macro that names just "array"
in the refusal, so the declared type is not the text php prints. A parameter
typed exactly `array` has no such ambiguity. Until the exemption was narrowed to
unions, the family with no check of its own answered nothing at all —
sort($notAnArray) and its seven relatives returned `false`, which is also what
they return for a sort that failed; call_user_func_array() returned false;
iterator_apply() RAN the callback; and getopt/hash/password_hash/unserialize/
fputcsv carried on with a string where an options array was declared. The
builtins that DO check word it identically, so the screen only pre-empts them —
and corrects one detail on the way: their ph7_type_name() said "bool" where php
names the VALUE, `true` or `false`.
--FILE--
<?php
function apsShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}

/* The sort family: by-reference `array &$array`, and no check of its own. */
foreach (['sort', 'rsort', 'ksort', 'krsort', 'asort', 'arsort', 'shuffle',
          'natsort', 'natcasesort'] as $apsFn) {
    apsShow($apsFn, function () use ($apsFn) { $v = 'nope'; return $apsFn($v); });
}
foreach (['usort', 'uasort', 'uksort'] as $apsFn) {
    apsShow($apsFn, function () use ($apsFn) {
        $v = 'nope';
        return $apsFn($v, fn($a, $b) => 0);
    });
}

/* Every other scalar shape, and php's VALUE name for a bool. */
apsShow('sort int', function () { $v = 5; return sort($v); });
apsShow('sort float', function () { $v = 1.5; return sort($v); });
apsShow('sort true', function () { $v = true; return sort($v); });
apsShow('sort false', function () { $v = false; return sort($v); });
apsShow('array_keys true', fn() => array_keys(true));
apsShow('in_array false', fn() => in_array(1, false));

/* An `array` parameter that is not the first one, and the ?array shape. */
apsShow('call_user_func_array', fn() => call_user_func_array('strlen', 'x'));
apsShow('forward_static_call_array', fn() => forward_static_call_array('strlen', 'x'));
apsShow('iterator_apply', fn() => iterator_apply(new ArrayIterator([1]), fn() => false, 'x'));
apsShow('getopt', fn() => getopt('a', 'x'));
apsShow('hash', fn() => hash('md5', 'd', false, 'x'));
apsShow('unserialize', fn() => unserialize('i:1;', 'x'));
apsShow('password_needs_rehash', fn() => password_needs_rehash('h', PASSWORD_DEFAULT, 'x'));
apsShow('vsprintf', fn() => vsprintf('%s', 'x'));

/* A UNION keeps its exemption: `array|object` is Z_PARAM_ARRAY_OR_OBJECT and
 * php words it from the builtin's own check, `array|string` takes the string,
 * and `Countable|array` takes anything countable. */
apsShow('array_walk union', function () { $v = 'nope'; return array_walk($v, fn() => 1); });
apsShow('implode union', fn() => implode('-', [1, 2]));
apsShow('count union', fn() => count('nope'));
apsShow('filter_var union', fn() => filter_var('1', FILTER_VALIDATE_INT, 0));

/* A by-reference parameter settles the REFERENCE question first: php binds the
 * argument at the call, before the callee's ZPP runs, so a literal is
 * "could not be passed by reference" and never a TypeError about its type. The
 * screen stands aside for a by-ref parameter with nothing to write back
 * through and lets that check speak. */
apsShow('byref literal', fn() => array_pop('nope'));
apsShow('byref variable', function () { $v = 'nope'; return array_pop($v); });

/* Nothing that used to work stops working. */
apsShow('sort ok', function () { $v = [3, 1, 2]; sort($v); return $v; });
apsShow('iterator_apply ok', fn() => iterator_apply(new ArrayIterator([1, 2]), fn() => false));
apsShow('call_user_func_array ok', fn() => call_user_func_array('strlen', ['abc']));
apsShow('unserialize ok', fn() => unserialize('i:7;', ['allowed_classes' => false]));
--EXPECT--
sort => TypeError: sort(): Argument #1 ($array) must be of type array, string given
rsort => TypeError: rsort(): Argument #1 ($array) must be of type array, string given
ksort => TypeError: ksort(): Argument #1 ($array) must be of type array, string given
krsort => TypeError: krsort(): Argument #1 ($array) must be of type array, string given
asort => TypeError: asort(): Argument #1 ($array) must be of type array, string given
arsort => TypeError: arsort(): Argument #1 ($array) must be of type array, string given
shuffle => TypeError: shuffle(): Argument #1 ($array) must be of type array, string given
natsort => TypeError: natsort(): Argument #1 ($array) must be of type array, string given
natcasesort => TypeError: natcasesort(): Argument #1 ($array) must be of type array, string given
usort => TypeError: usort(): Argument #1 ($array) must be of type array, string given
uasort => TypeError: uasort(): Argument #1 ($array) must be of type array, string given
uksort => TypeError: uksort(): Argument #1 ($array) must be of type array, string given
sort int => TypeError: sort(): Argument #1 ($array) must be of type array, int given
sort float => TypeError: sort(): Argument #1 ($array) must be of type array, float given
sort true => TypeError: sort(): Argument #1 ($array) must be of type array, true given
sort false => TypeError: sort(): Argument #1 ($array) must be of type array, false given
array_keys true => TypeError: array_keys(): Argument #1 ($array) must be of type array, true given
in_array false => TypeError: in_array(): Argument #2 ($haystack) must be of type array, false given
call_user_func_array => TypeError: call_user_func_array(): Argument #2 ($args) must be of type array, string given
forward_static_call_array => TypeError: forward_static_call_array(): Argument #2 ($args) must be of type array, string given
iterator_apply => TypeError: iterator_apply(): Argument #3 ($args) must be of type ?array, string given
getopt => TypeError: getopt(): Argument #2 ($long_options) must be of type array, string given
hash => TypeError: hash(): Argument #4 ($options) must be of type array, string given
unserialize => TypeError: unserialize(): Argument #2 ($options) must be of type array, string given
password_needs_rehash => TypeError: password_needs_rehash(): Argument #3 ($options) must be of type array, string given
vsprintf => TypeError: vsprintf(): Argument #2 ($values) must be of type array, string given
array_walk union => TypeError: array_walk(): Argument #1 ($array) must be of type array, string given
implode union => '1-2'
count union => TypeError: count(): Argument #1 ($value) must be of type Countable|array, string given
filter_var union => 1
byref literal => Error: array_pop(): Argument #1 ($array) could not be passed by reference
byref variable => TypeError: array_pop(): Argument #1 ($array) must be of type array, string given
sort ok => array (  0 => 1,  1 => 2,  2 => 3,)
iterator_apply ok => 1
call_user_func_array ok => 3
unserialize ok => 7
