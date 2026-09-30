--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A closure LITERAL is not dereferencable unless it is parenthesised
--DESCRIPTION--
php refuses `(`, `[`, `->`, `?->` and `::` directly after a closure or arrow-fn
literal, which is why every IIFE in the wild is written `(function () {…})()`.
PHL accepted all five — a silent acceptance of what php REFUSES, and the reason
it had to be fixed beside the statement-position rule: that rule sends
`function () {…};` down the expression path, and without this screen
`function () {…}();` would have become the one shape php rejects and PHL runs.
Each snippet is linted in a process of its own through PHP_BINARY, so both
engines answer with their own parser and the test asserts the VERDICT rather
than the wording.
--FILE--
<?php
$dir = sys_get_temp_dir() . '/phlt-deref-' . getmypid();
@mkdir($dir);

$cases = [
    'call a closure literal'        => '$x = function () { return 1; }();',
    'call an arrow-fn literal'      => '$x = (fn () => 1)(); $y = fn () => 1; $z = $y();',
    'subscript a closure literal'   => '$x = function () { return [1]; }[0];',
    'arrow off a closure literal'   => '$x = function () { return new stdClass; }->p;',
    'nullsafe off a closure'        => '$x = function () { return new stdClass; }?->p;',
    'paamayim off a closure'        => '$x = function () {}::class;',
    'statement-position closure'    => 'function () {};',
    'statement-position arrow fn'   => 'fn ($a) => $a;',
    'statement-position by-ref'     => 'function &() {};',
    'parenthesised IIFE'            => '$x = (function () { return 1; })();',
    'parenthesised, then subscript' => '$x = (fn () => [1])()[0];',
    'closure held in a variable'    => '$f = function () { return 1; }; $x = $f();',
];

$n = 0;
foreach ($cases as $label => $src) {
    $file = $dir . '/case' . $n++ . '.php';
    file_put_contents($file, "<?php\n" . $src . "\n");
    $out = [];
    $rc = 0;
    exec(escapeshellarg(PHP_BINARY) . ' -l ' . escapeshellarg($file) . ' 2>&1', $out, $rc);
    echo str_pad($label, 32), $rc === 0 ? 'accepted' : 'REFUSED', "\n";
    @unlink($file);
}
@rmdir($dir);
--EXPECT--
call a closure literal          REFUSED
call an arrow-fn literal        accepted
subscript a closure literal     REFUSED
arrow off a closure literal     REFUSED
nullsafe off a closure          REFUSED
paamayim off a closure          REFUSED
statement-position closure      accepted
statement-position arrow fn     accepted
statement-position by-ref       accepted
parenthesised IIFE              accepted
parenthesised, then subscript   accepted
closure held in a variable      accepted
