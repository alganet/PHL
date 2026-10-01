--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A bare `static` in an expression needs its `::`, `new` or `instanceof`
--DESCRIPTION--
php's grammar takes the keyword in an expression in three places only: before
`::`, and as the class of `new` and of `instanceof` (the closure forms are
their own productions). Anywhere else its parser refuses the file at compile
time; PHL compiled `$x = static;` and failed at RUNTIME on an undefined
constant "static". Each snippet is linted in a process of its own through
PHP_BINARY, so both engines answer with their own parser and the test asserts
the VERDICT rather than the wording.
--FILE--
<?php
$dir = sys_get_temp_dir() . '/phlt-static-' . getmypid();
@mkdir($dir);

$cases = [
    'before the scope operator' => 'class A { static function f() { return static::class; } }',
    'new static'                => 'class A { static function f() { return new static; } }',
    'new static with arguments' => 'class A { static function f() { return new static(1); } }',
    'instanceof static'         => 'class A { function f($o) { return $o instanceof static; } }',
    'static closure'            => '$f = static function () { return 1; };',
    'static arrow function'     => '$f = static fn () => 1;',
    'static variable'           => 'function f() { static $n = 0; return ++$n; }',
    'static property'           => 'class A { public static $p = 1; }',
    'a constant named STATIC'   => 'class A { const STATIC = 1; } echo A::STATIC;',
    'assigned bare'             => '$x = static;',
    'returned bare'             => 'function f() { return static; }',
    'returned bare by a method' => 'class A { function f() { return static; } }',
    'echoed bare'               => 'echo static;',
    'in arithmetic'             => 'echo static + 1;',
    'called bare'               => 'echo static();',
    'as an argument'            => 'var_dump(static);',
];

$n = 0;
foreach ($cases as $label => $src) {
    $file = $dir . '/case' . $n++ . '.php';
    file_put_contents($file, "<?php\n" . $src . "\n");
    $out = [];
    $rc = 0;
    exec(escapeshellarg(PHP_BINARY) . ' -l ' . escapeshellarg($file) . ' 2>&1', $out, $rc);
    echo str_pad($label, 28), $rc === 0 ? 'accepted' : 'REFUSED', "\n";
    @unlink($file);
}
@rmdir($dir);
?>
--EXPECT--
before the scope operator   accepted
new static                  accepted
new static with arguments   accepted
instanceof static           accepted
static closure              accepted
static arrow function       accepted
static variable             accepted
static property             accepted
a constant named STATIC     accepted
assigned bare               REFUSED
returned bare               REFUSED
returned bare by a method   REFUSED
echoed bare                 REFUSED
in arithmetic               REFUSED
called bare                 REFUSED
as an argument              REFUSED
