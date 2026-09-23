--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A named argument to a builtin binds to the parameter it names
--DESCRIPTION--
A compiled function binds `name:` arguments from its parameter records; a host
function and a native method have none, so every named argument stayed where it
was WRITTEN. Reordering reported a TypeError against the wrong parameter
(`str_pad(length: 5, string: "x")`), and SKIPPING one was silent and wrong:
`str_pad("x", 5, pad_type: STR_PAD_LEFT)` bound the flag as $pad_string and
answered "x0000" where php answers "    x". The declared signature is the source
of names, defaults and positions — the same string Reflection prints — so a
skipped parameter now takes its default, an unknown name and a duplicate raise
php's Errors, and a gap with no default is php's "Argument #N ($name) not
passed".
--FILE--
<?php
function nabnShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
/* Reordered, skipped, and mixed with positional arguments. */
nabnShow('reorder', fn() => str_pad(length: 5, string: 'x'));
nabnShow('skip', fn() => str_pad('x', 5, pad_type: STR_PAD_LEFT));
nabnShow('reorder2', fn() => str_repeat(times: 3, string: 'ab'));
nabnShow('implode', fn() => implode(array: ['a', 'b'], separator: '-'));
nabnShow('in order', fn() => str_pad(string: 'x', length: 5, pad_string: '-'));
nabnShow('positional then named', fn() => str_pad('x', length: 5, pad_string: '-'));
nabnShow('substr skip', fn() => substr('abcdef', offset: 2));
nabnShow('substr reorder', fn() => substr(length: 2, string: 'abcdef', offset: 1));
nabnShow('in_array strict', fn() => in_array(strict: true, needle: '1', haystack: [1]));
nabnShow('range step', fn() => range(start: 0, end: 10, step: 5));
nabnShow('array_fill', fn() => array_fill(count: 2, start_index: 1, value: 'x'));
nabnShow('date', fn() => date(timestamp: 0, format: 'Y-m-d'));

/* A by-reference parameter still writes back through its NAME. */
nabnShow('by-ref named', function () {
    $m = null;
    $r = preg_match(subject: 'abc', pattern: '/b/', matches: $m);
    return [$r, $m];
});
nabnShow('sort named', function () { $a = [3, 1, 2]; sort(array: $a); return $a; });

/* php's refusals. */
nabnShow('unknown name', fn() => str_pad('x', 5, nope: 1));
nabnShow('duplicate', fn() => str_pad('x', string: 'y', length: 2));
nabnShow('gap with no default', function () {
    $d = new DateTime('2020-01-01 10:20:30');
    return $d->setTime(hour: 1, second: 5)->format('H:i:s');
});

/* A native METHOD reads the same signature. */
nabnShow('method named', function () {
    $d = new DateTime('2020-01-01');
    return $d->setDate(year: 2021, month: 2, day: 3)->format('Y-m-d');
});
nabnShow('method skip', function () {
    $d = new DateTime('2020-01-01 10:20:30');
    return $d->setTime(hour: 5, minute: 6)->format('H:i:s');
});
nabnShow('static method', fn() => DateTime::createFromFormat(format: 'Y-m-d', datetime: '2020-05-06')->format('Y-m-d'));
echo "end\n";
?>
--EXPECT--
reorder => 'x    '
skip => '    x'
reorder2 => 'ababab'
implode => 'a-b'
in order => 'x----'
positional then named => 'x----'
substr skip => 'cdef'
substr reorder => 'bc'
in_array strict => false
range step => array (  0 => 0,  1 => 5,  2 => 10,)
array_fill => array (  1 => 'x',  2 => 'x',)
date => '1970-01-01'
by-ref named => array (  0 => 1,  1 =>   array (    0 => 'b',  ),)
sort named => array (  0 => 1,  1 => 2,  2 => 3,)
unknown name => Error: Unknown named parameter $nope
duplicate => Error: Named parameter $string overwrites previous argument
gap with no default => ArgumentCountError: DateTime::setTime(): Argument #2 ($minute) not passed
method named => '2021-02-03'
method skip => '05:06:00'
static method => '2020-05-06'
end
