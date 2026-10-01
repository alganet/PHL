--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A named argument that SKIPS a builtin parameter gets that parameter's real default
--DESCRIPTION--
A builtin's parameters live in a php-style SIGNATURE STRING, and its defaults
are the TEXT in it. Two doors read that text: ReflectionParameter::
getDefaultValue(), and the named-argument binder, which has to materialize
every parameter a named call stepped over. The binder carried a reader of its
own that knew null/true/false, `[]`, a quoted string and a DECIMAL number, so
the two doors disagreed about every other spelling: mkdir()'s `0777` bound as
seven hundred and seventy-seven (a directory created mode 01411, which the next
chdir() could not enter -- found in monolog's suite), and a default written as
a CONSTANT -- `ENT_QUOTES | ENT_SUBSTITUTE | ENT_HTML401`, `SORT_REGULAR`,
`PDO::PARAM_STR`, `SplFileObject::class`, ~180 parameters in all -- reported
the parameter as NOT PASSED, so php's own
`htmlspecialchars("<a>", encoding: 'UTF-8')` was an ArgumentCountError here.
There is one reader now. `[]` and the `?` marker stay with the binder, because
neither is a scalar the shared reader answers.
--FILE--
<?php
/*
 * Every row names a LATER parameter, so the engine has to materialize the
 * skipped one from the signature's default TEXT.
 */
function show(string $label, callable $f): void
{
    try {
        $r = $f();
    } catch (\Throwable $e) {
        $r = get_class($e) . ': ' . $e->getMessage();
    }
    printf("%-34s %s\n", $label, var_export($r, true));
}

/* A default written in a RADIX. Compared against the same mode spelled
 * positionally, so the answer does not depend on the umask or on whether the
 * platform has unix modes at all. */
$root = sys_get_temp_dir() . '/phl-sigdef-' . getmypid();
show('0777, named vs positional', function () use ($root) {
    $a = "$root/pos/deep";
    $b = "$root/named/deep";
    $ok = mkdir($a, 0777, true) && mkdir($b, recursive: true);
    clearstatcache();
    return $ok && is_dir($b) && fileperms($a) === fileperms($b);
});
foreach (['/named/deep', '/named', '/pos/deep', '/pos', ''] as $leaf) {
    @rmdir($root . $leaf);
}

/* A default that is a CONSTANT expression: a single name, an OR of several, a
 * class constant, and `C::class`. */
show('ENT_QUOTES|ETC (htmlspecialchars)', fn() => htmlspecialchars('<a>&"\'', double_encode: false));
show('ENT_QUOTES|ETC (decode)',          fn() => html_entity_decode('&lt;&amp;&quot;&#039;', encoding: 'UTF-8'));
show('HTML_SPECIALCHARS',              fn() => count(get_html_translation_table(flags: ENT_QUOTES)));
show('FILTER_DEFAULT',                 fn() => filter_var('42', options: 0));
show('SCANDIR_SORT_ASCENDING',         fn() => scandir('.', context: null) === scandir('.'));
// an oracle built without ext/fileinfo (CI's Windows php) has no class to ask
show('FILEINFO_NONE',                  fn() => extension_loaded('fileinfo')
    ? get_class(new finfo(magic_database: null)) : 'finfo');
show('DEBUG_BACKTRACE_PROVIDE_OBJECT', function () {
    $f = fn() => count(debug_backtrace(limit: 1));
    return $f();
});
show('SimpleXMLElement::class',        fn() => (string) simplexml_load_string('<r>x</r>', options: 0));
show('RecursiveIteratorIterator',      fn() => iterator_to_array(
    new RecursiveIteratorIterator(new RecursiveArrayIterator([1, [2, 3]]), flags: 0),
    false
));
show('RegexIterator::MATCH',           fn() => iterator_to_array(
    new RegexIterator(new ArrayIterator(['aa', 'bb']), '/a/', flags: 0),
    false
));

/* The two shapes that are NOT reduced, and must keep answering what they did:
 * `[]` builds an empty array, and a default the table cannot state (`?`)
 * reports the parameter as not passed -- which is php's own answer for
 * array_splice()'s $length. */
show('[] (hash options)',              fn() => hash('crc32b', data: 'abc'));
show('? (array_splice length)',        function () {
    $a = [1, 2, 3, 4];
    $cut = array_splice($a, 1, replacement: [9]);
    return [$cut, $a];
});

/* A named call that skips nothing still binds by position. */
show('no skip (str_pad)',              fn() => str_pad('x', 5, pad_type: STR_PAD_LEFT));
show('no skip (round)',                fn() => round(1.2345, precision: 2));
--EXPECT--
0777, named vs positional          true
ENT_QUOTES|ETC (htmlspecialchars)  '&lt;a&gt;&amp;&quot;&#039;'
ENT_QUOTES|ETC (decode)            '<&"\''
HTML_SPECIALCHARS                  5
FILTER_DEFAULT                     '42'
SCANDIR_SORT_ASCENDING             true
FILEINFO_NONE                      'finfo'
DEBUG_BACKTRACE_PROVIDE_OBJECT     1
SimpleXMLElement::class            'x'
RecursiveIteratorIterator          array (
  0 => 1,
  1 => 2,
  2 => 3,
)
RegexIterator::MATCH               array (
  0 => 'aa',
)
[] (hash options)                  '352441c2'
? (array_splice length)            array (
  0 => 
  array (
    0 => 2,
    1 => 3,
    2 => 4,
  ),
  1 => 
  array (
    0 => 1,
    1 => 9,
  ),
)
no skip (str_pad)                  '    x'
no skip (round)                    1.23
