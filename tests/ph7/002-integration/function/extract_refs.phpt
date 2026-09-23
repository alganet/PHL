--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
extract: EXTR_REFS binds every imported name BY REFERENCE to its array slot
--FILE--
<?php
var_dump(defined('EXTR_REFS'), EXTR_REFS);

// The imported name aliases the element: writing through it updates the array.
$exr = ['a' => 1];
var_dump(extract($exr, EXTR_REFS | EXTR_OVERWRITE));
$a = 99;
var_dump($exr['a'], $exr);

// The mode in the low byte keeps deciding which names are imported at all.
$exs = ['z' => 1];
$z = 'keep';
var_dump(extract($exs, EXTR_REFS | EXTR_SKIP), $z, $exs['z']);

$exi = ['w' => 5];
$w = 0;
var_dump(extract($exi, EXTR_REFS | EXTR_IF_EXISTS));
$w = 42;
var_dump($exi['w']);

$exp = ['q' => 1];
$q = 'existing';
var_dump(extract($exp, EXTR_REFS | EXTR_PREFIX_SAME, 'p'));
$p_q = 7;
var_dump($exp['q'], $q);

// A numeric key and an invalid name are dropped by EXTR_REFS exactly as by value.
$exk = [0 => 'zero', 'ok' => 1, 'bad name' => 2];
var_dump(extract($exk, EXTR_REFS), isset($ok));

// ...and the low byte is still validated first.
try {
    extract($exr, EXTR_REFS | EXTR_PREFIX_SAME);
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
bool(true)
int(256)
int(1)
int(99)
array(1) {
  ["a"]=>
  &int(99)
}
int(0)
string(4) "keep"
int(1)
int(1)
int(42)
int(1)
int(7)
string(8) "existing"
int(1)
bool(true)
extract(): Argument #3 ($prefix) is required when using this extract type
--CLEAN--
<?php
unset($exr, $a, $exs, $z, $exi, $w, $exp, $q, $p_q, $exk, $ok);
