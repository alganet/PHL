--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
extract: every mode's collision/prefix/numeric-key/invalid-name behaviour
--FILE--
<?php
// One frame per run so the imported names cannot leak between modes.
$exm_run = function (int $flags, ?string $prefix = null) {
    $a = 'pre-a';
    $c = 'pre-c';
    $src = ['a' => 1, 'b' => 2, 5 => 'num', 'bad name' => 'x', '' => 'empty', '1abc' => 'y'];
    $n = $prefix === null ? extract($src, $flags) : extract($src, $flags, $prefix);
    $vars = get_defined_vars();
    unset($vars['flags'], $vars['prefix'], $vars['src'], $vars['n']);
    ksort($vars);
    return "n=$n " . json_encode($vars);
};
echo "OVERWRITE:        ", $exm_run(EXTR_OVERWRITE), "\n";
echo "SKIP:             ", $exm_run(EXTR_SKIP), "\n";
echo "PREFIX_SAME:      ", $exm_run(EXTR_PREFIX_SAME, 'p'), "\n";
echo "PREFIX_ALL:       ", $exm_run(EXTR_PREFIX_ALL, 'p'), "\n";
echo "PREFIX_INVALID:   ", $exm_run(EXTR_PREFIX_INVALID, 'p'), "\n";
echo "PREFIX_IF_EXISTS: ", $exm_run(EXTR_PREFIX_IF_EXISTS, 'p'), "\n";
echo "IF_EXISTS:        ", $exm_run(EXTR_IF_EXISTS), "\n";
// An empty prefix still inserts php's separator, and a prefixed name that is
// not an identifier ("z_-3") is dropped instead of installed.
$exm_edge = function () {
    $n = extract(['q' => 1], EXTR_PREFIX_ALL, '');
    $m = extract(['-3' => 'neg', '5' => 'five'], EXTR_PREFIX_ALL, 'z');
    return "$n|$m|" . ($_q ?? 'MISS') . '|' . ($z_5 ?? 'MISS') . '|' . (${'z_-3'} ?? 'MISS');
};
echo "edge:             ", $exm_edge(), "\n";
?>
--EXPECT--
OVERWRITE:        n=2 {"a":1,"b":2,"c":"pre-c"}
SKIP:             n=1 {"a":"pre-a","b":2,"c":"pre-c"}
PREFIX_SAME:      n=2 {"a":"pre-a","b":2,"c":"pre-c","p_a":1}
PREFIX_ALL:       n=4 {"a":"pre-a","c":"pre-c","p_1abc":"y","p_5":"num","p_a":1,"p_b":2}
PREFIX_INVALID:   n=5 {"a":1,"b":2,"c":"pre-c","p_":"empty","p_1abc":"y","p_5":"num"}
PREFIX_IF_EXISTS: n=1 {"a":"pre-a","c":"pre-c","p_a":1}
IF_EXISTS:        n=1 {"a":1,"c":"pre-c"}
edge:             1|1|1|five|MISS
--CLEAN--
<?php
