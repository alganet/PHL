--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed parameter: folded defaults php accepts at compile time, and the ones it never folds
--FILE--
<?php
const PDCA_STR = "a";
function pdca_div(int $x = 4 / 2) { var_dump($x); }
function pdca_wide(float $x = 1 + 2) { var_dump($x); }
function pdca_union(int|float $x = 1.5) { var_dump($x); }
function pdca_iter(iterable $x = [1, [2]]) { var_dump($x); }
function pdca_true(true $x = 1 < 2) { var_dump($x); }
function pdca_null(?string $x = null) { var_dump($x); }
function pdca_mixed(mixed $x = E_ERROR) { var_dump($x); }
function pdca_line(int $x = __LINE__) { var_dump($x); }
// A folded null is the implicit-nullable form for a plain parameter.
function pdca_folded_null(int $x = 1 ? null : 2) { var_dump($x); }
// Never folded: a constant name, a throwing operation, an object.
function pdca_const(int $x = PDCA_STR) {}
function pdca_intmax(array $x = PHP_INT_MAX + 1) {}
function pdca_mod(int $x = 1 % 0) {}
function pdca_new(int $x = new stdClass) {}
pdca_div(); pdca_wide(); pdca_union(); pdca_iter(); pdca_true();
pdca_null(); pdca_mixed(); pdca_line(); pdca_folded_null(5);
echo "done\n";
--EXPECT--
int(2)
float(3)
float(1.5)
array(2) {
  [0]=>
  int(1)
  [1]=>
  array(1) {
    [0]=>
    int(2)
  }
}
bool(true)
NULL
int(1)
int(10)
int(5)
done
--CLEAN--
<?php
