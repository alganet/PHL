--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A namespace separator binds only to the label glued to it
--DESCRIPTION--
php's scanner makes `A\B` and `\A\B` ONE token each, so a `\` with a space on
either side, or nothing after it, reaches its parser as a lone separator that
the grammar takes in exactly one place: before the `{` of a group `use`. PHL
walked names token by token and never looked at the bytes around a separator,
so `namespace A\ B;`, `A \B` and `A\;` all compiled as the name `A\B`.
Each snippet is linted in a process of its own through PHP_BINARY, so both
engines answer with their own parser and the test asserts the VERDICT rather
than the wording.
--FILE--
<?php
$dir = sys_get_temp_dir() . '/phlt-nssep-' . getmypid();
@mkdir($dir);

$cases = [
    'space after the separator'         => 'namespace A\ B;',
    'space before the separator'        => 'namespace A \B;',
    'spaces on both sides'              => 'echo A \ B;',
    'trailing separator'                => 'namespace A\B\;',
    'a name after a name'               => 'echo A \B::X;',
    'a name after a variable'           => '$a = 1; echo $a \B::X;',
    'leading separator alone'           => 'echo \ B::X;',
    'separator before a call paren'     => 'echo strlen \ ("x");',
    'group use without its separator'   => 'use A\B{C};',
    'fully qualified namespace name'    => 'namespace \A;',
    'extends with a spaced name'        => 'class A extends B\ C {}',
    'parameter type with a spaced name' => 'function f(A\ B $x) {}',
    'qualified name'                    => 'namespace A\B; echo \A\B\C::class;',
    'fully qualified call'              => 'echo \strlen("x");',
    'group use'                         => 'use A\B\{C, D};',
    'group use with a spaced separator' => 'use A\B \ {C, D};',
    'name after a keyword'              => 'function f(): \A\B { return new \A\B; }',
    'name after readonly'               => 'class P { public readonly \DateTime $d; }',
    'name after a comment line'         => "return\n// note\n\\strlen('a');",
    'relative name operator'            => 'namespace A; namespace\f();',
    'name after instanceof'             => '$x = null; var_dump($x instanceof \Foo);',
];

$n = 0;
foreach ($cases as $label => $src) {
    $file = $dir . '/case' . $n++ . '.php';
    file_put_contents($file, "<?php\n" . $src . "\n");
    $out = [];
    $rc = 0;
    exec(escapeshellarg(PHP_BINARY) . ' -l ' . escapeshellarg($file) . ' 2>&1', $out, $rc);
    echo str_pad($label, 36), $rc === 0 ? 'accepted' : 'REFUSED', "\n";
    @unlink($file);
}
@rmdir($dir);
?>
--EXPECT--
space after the separator           REFUSED
space before the separator          REFUSED
spaces on both sides                REFUSED
trailing separator                  REFUSED
a name after a name                 REFUSED
a name after a variable             REFUSED
leading separator alone             REFUSED
separator before a call paren       REFUSED
group use without its separator     REFUSED
fully qualified namespace name      REFUSED
extends with a spaced name          REFUSED
parameter type with a spaced name   REFUSED
qualified name                      accepted
fully qualified call                accepted
group use                           accepted
group use with a spaced separator   accepted
name after a keyword                accepted
name after readonly                 accepted
name after a comment line           accepted
relative name operator              accepted
name after instanceof               accepted
