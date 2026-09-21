--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_callable() writes php's &$callable_name, callable or not
--FILE--
<?php
// php names the value whether or not it turns out callable — the name describes
// the INPUT, it is not a resolution result.
function is_callable_name_probe($v, $syntax_only = false) {
    $n = 'PRESET';
    is_callable($v, $syntax_only, $n);
    var_export($n);
    echo "\n";
}
class IsCallableNameHost {
    public function m() {}
    public static function s() {}
}
$host = new IsCallableNameHost();

// A [target, method] pair: the target verbatim (a class-name string as written,
// an object by its class name) and the method verbatim — no case folding.
is_callable_name_probe([$host, 'm']);
is_callable_name_probe(['IsCallableNameHost', 's']);
is_callable_name_probe([$host, 'M']);
// Named even when the class or the method does not exist.
is_callable_name_probe(['NoSuchClassAtAll', 'm']);
is_callable_name_probe([$host, 'nope']);
// A non-callable object is described through its (missing) __invoke.
is_callable_name_probe(new stdClass());
// Plain strings pass through, including the "Class::method" form.
is_callable_name_probe('strlen');
is_callable_name_probe('IsCallableNameHost::s');
is_callable_name_probe('no_such_function_at_all');
is_callable_name_probe('');
// Anything else is the plain string cast; an array of the wrong shape is "Array".
is_callable_name_probe(5);
is_callable_name_probe(5.5);
is_callable_name_probe(true);
is_callable_name_probe(null);
is_callable_name_probe([]);
is_callable_name_probe([1, 2]);
is_callable_name_probe(['IsCallableNameHost']);
is_callable_name_probe(['IsCallableNameHost', 's', 'extra']);
// $syntax_only does not change the name.
is_callable_name_probe([$host, 'nope'], true);
// A first-class callable names the function it wraps, qualified by its class.
is_callable_name_probe($host->m(...));
is_callable_name_probe(IsCallableNameHost::s(...));
is_callable_name_probe(strlen(...));
is_callable_name_probe(Closure::fromCallable([$host, 'm']));
// The out-param reaches an array element and a property too.
$arr = [];
is_callable('strlen', false, $arr['k']);
var_export($arr);
echo "\n";
?>
--EXPECT--
'IsCallableNameHost::m'
'IsCallableNameHost::s'
'IsCallableNameHost::M'
'NoSuchClassAtAll::m'
'IsCallableNameHost::nope'
'stdClass::__invoke'
'strlen'
'IsCallableNameHost::s'
'no_such_function_at_all'
''
'5'
'5.5'
'1'
''
'Array'
'Array'
'Array'
'Array'
'IsCallableNameHost::nope'
'IsCallableNameHost::m'
'IsCallableNameHost::s'
'strlen'
'IsCallableNameHost::m'
array (
  'k' => 'strlen',
)
--CLEAN--
<?php
