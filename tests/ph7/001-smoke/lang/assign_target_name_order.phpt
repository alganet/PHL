--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An assignment target's dynamic subscript and property names run before the assigned value
--FILE--
<?php
function atno_t($s) { echo "[$s]"; return $s; }

echo "1: "; $a = []; $a[atno_t('k')] = atno_t('v'); var_dump($a);
echo "2: "; $a = []; $a[atno_t('1')][atno_t('2')][atno_t('3')] = atno_t('V'); echo "\n";
echo "3: "; $a = []; $a[atno_t('k')] = $a[atno_t('j')] = atno_t('v'); echo "\n";
echo "4: "; $a = ['k' => 'x']; $a[atno_t('k')] .= atno_t('v'); var_dump($a);
echo "5: "; $o = new stdClass; $o->{atno_t('p')} = atno_t('v'); var_dump($o->p);
echo "6: "; $o = new stdClass; $o->{atno_t('p')}[atno_t('k')] = atno_t('v'); echo "\n";

class AtnoStatic { public static $a = []; }
echo "7: "; AtnoStatic::$a[atno_t('k')] = atno_t('v'); var_dump(AtnoStatic::$a);

// A plain variable name is read at the FETCH, which is after the value: php keeps it
// as the fetch opline's own operand rather than materializing it first.
echo "8: "; $a = []; $k = 'A';
$a[$k] = (function () use (&$k) { $k = 'B'; return 'V'; })();
var_dump($a);

// Anything else is materialized where it is written, so it does NOT see the value's write.
echo "9: "; $a = []; $n = 3;
$a[$n + 1] = (function () use (&$n) { $n = 99; return 'V'; })();
var_dump($a);

// The target is untouched while the value runs: a recursive memoization must not find
// its own key already seeded.
echo "10: "; $a = [];
$a[atno_t('k')] = (function () use (&$a) { var_dump(array_key_exists('k', $a)); return 'V'; })();
echo "11: "; $a = [];
$a[atno_t('k')][atno_t('j')] = (function () use (&$a) { var_dump(array_key_exists('k', $a)); return 'V'; })();

// A throw from the value leaves the names already run and nothing written.
echo "12: "; $a = [];
try {
    $a[atno_t('k')] = (function () { throw new Exception('boom'); })();
} catch (Exception $e) {
    echo "caught ";
}
var_dump($a);
?>
--EXPECT--
1: [k][v]array(1) {
  ["k"]=>
  string(1) "v"
}
2: [1][2][3][V]
3: [k][j][v]
4: [k][v]array(1) {
  ["k"]=>
  string(2) "xv"
}
5: [p][v]string(1) "v"
6: [p][k][v]
7: [k][v]array(1) {
  ["k"]=>
  string(1) "v"
}
8: array(1) {
  ["B"]=>
  string(1) "V"
}
9: array(1) {
  [4]=>
  string(1) "V"
}
10: [k]bool(false)
11: [k][j]bool(false)
12: [k]caught array(0) {
}
