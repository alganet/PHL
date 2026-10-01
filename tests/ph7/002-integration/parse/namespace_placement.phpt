--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Where a namespace declaration may stand, and what may stand around it
--DESCRIPTION--
php's rules, in the order its compiler applies them: the bracketed and the
plain form never mix in one file, a block never nests, the FIRST declaration
of either form must be the first statement (declares and empty statements
aside), and once a file has used the bracketed form nothing but another
`namespace` or the halt may stand outside a block. A bare `namespace;` is not
a form at all. PHL compiled every one of these and ran the program.
Each snippet is linted in a process of its own through PHP_BINARY, so both
engines answer with their own compiler and the test asserts the VERDICT
rather than the wording.
--FILE--
<?php
$dir = sys_get_temp_dir() . '/phlt-nsplace-' . getmypid();
@mkdir($dir);

$cases = [
    'first statement'             => 'namespace A; echo 1;',
    'after a declare'             => 'declare(strict_types=1); namespace A;',
    'after an empty statement'    => '; namespace A;',
    'two plain declarations'      => 'namespace A; echo 1; namespace B; echo 2;',
    'two blocks'                  => 'namespace A { echo 1; } namespace { echo 2; }',
    'block then the halt'         => 'namespace A { } __halt_compiler(); data',
    'after code'                  => 'echo 1; namespace A;',
    'block after code'            => 'echo 1; namespace A { }',
    'plain then block'            => 'namespace A; namespace B { }',
    'block then plain'            => 'namespace A { } namespace B;',
    'nested blocks'               => 'namespace A { namespace B { } }',
    'code outside the block'      => 'namespace A { } echo 1;',
    'declare outside the block'   => 'namespace A { } declare(ticks=1);',
    'bare namespace'              => 'namespace;',
    'strict_types after it'       => 'namespace A; declare(strict_types=1);',
];

$n = 0;
foreach ($cases as $label => $src) {
    $file = $dir . '/case' . $n++ . '.php';
    file_put_contents($file, "<?php\n" . $src . "\n");
    $out = [];
    $rc = 0;
    exec(escapeshellarg(PHP_BINARY) . ' -l ' . escapeshellarg($file) . ' 2>&1', $out, $rc);
    echo str_pad($label, 30), $rc === 0 ? 'accepted' : 'REFUSED', "\n";
    @unlink($file);
}
@rmdir($dir);
?>
--EXPECT--
first statement               accepted
after a declare               accepted
after an empty statement      accepted
two plain declarations        accepted
two blocks                    accepted
block then the halt           accepted
after code                    REFUSED
block after code              REFUSED
plain then block              REFUSED
block then plain              REFUSED
nested blocks                 REFUSED
code outside the block        REFUSED
declare outside the block     REFUSED
bare namespace                REFUSED
strict_types after it         REFUSED
