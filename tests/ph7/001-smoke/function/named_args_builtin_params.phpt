--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Builtins implemented in the embedded prelude declare php's parameter NAMES
--FILE--
<?php
// The prelude-implemented builtins carried PH7's own parameter names ($str,
// $v, $what, $iFlags, $zDir...), which no arity or type check catches: a
// named argument that is valid php ("count_chars(string: ...)") died with
// "Unknown named parameter". Reflection reported the wrong names too.
$expect = [
    'array_replace_recursive'     => 'array,replacements',
    'array_unshift'               => 'array',
    'class_implements'            => 'object_or_class,autoload',
    'class_parents'               => 'object_or_class,autoload',
    'class_uses'                  => 'object_or_class,autoload',
    'count_chars'                 => 'string,mode',
    'dir'                         => 'directory',
    'doubleval'                   => 'value',
    'extension_loaded'            => 'extension',
    'fdiv'                        => 'num1,num2',
    'filegroup'                   => 'filename',
    'fileinode'                   => 'filename',
    'fileowner'                   => 'filename',
    'fileperms'                   => 'filename',
    'glob'                        => 'pattern,flags',
    'hex2bin'                     => 'string',
    'is_countable'                => 'value',
    'is_finite'                   => 'num',
    'is_infinite'                 => 'num',
    'is_iterable'                 => 'value',
    'is_nan'                      => 'num',
    'long2ip'                     => 'ip',
    'number_format'               => 'num,decimals,decimal_separator,thousands_separator',
    'preg_replace_callback_array' => 'pattern,subject,limit',
    'scandir'                     => 'directory,sorting_order',
    'session_regenerate_id'       => 'delete_old_session',
    'tempnam'                     => 'directory,prefix',
];
foreach ($expect as $fn => $want) {
    $got = [];
    foreach ((new ReflectionFunction($fn))->getParameters() as $p) {
        $got[] = $p->getName();
    }
    // php declares trailing parameters PHL has no consumer for ($context,
    // &$count, ...$values); compare only the prefix this engine implements.
    $got = implode(',', array_slice($got, 0, substr_count($want, ',') + 1));
    if ($got !== $want) {
        echo "$fn: got $got, want $want\n";
    }
}
echo "names ok\n";

// End to end: the same names actually bind as named arguments.
var_dump(count_chars(string: "hello", mode: 3));
var_dump(number_format(num: 1234.5, decimals: 1, decimal_separator: ",", thousands_separator: "."));
var_dump(hex2bin(string: "504850"));
var_dump(fdiv(num1: 1.0, num2: 0.0));
var_dump(long2ip(ip: 2130706433));
var_dump(doubleval(value: "3.5"), is_nan(num: NAN), is_finite(num: INF));
var_dump(is_iterable(value: []), is_countable(value: new ArrayObject()));
var_dump(extension_loaded(extension: "json"));
var_dump(class_implements(object_or_class: new ArrayObject(), autoload: true)["Countable"]);
// glob()'s RESULT is platform- and cwd-dependent; that the named arguments bind
// at all is the assertion (an unknown name is a fatal Error).
$g = glob(pattern: "nonexistent-phl-glob-entry-*", flags: GLOB_NOSORT);
var_dump(is_array($g) || $g === false);
var_dump(array_replace_recursive(array: ["a" => 1]));
var_dump(preg_replace_callback_array(pattern: ["/a/" => fn($m) => "b"], subject: "aaa"));
?>
--EXPECT--
names ok
string(4) "ehlo"
string(7) "1.234,5"
string(3) "PHP"
float(INF)
string(9) "127.0.0.1"
float(3.5)
bool(true)
bool(false)
bool(true)
bool(true)
bool(true)
string(9) "Countable"
bool(true)
array(1) {
  ["a"]=>
  int(1)
}
string(3) "bbb"
--CLEAN--
<?php
