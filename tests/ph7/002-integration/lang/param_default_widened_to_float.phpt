--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed parameter: an int default let into a float is stored as a float
--FILE--
<?php
declare(strict_types=1);
const PDWF_K = 7;
interface PdwfI { function m(float $a = 1); }
abstract class PdwfA { abstract function n(?float $b = 2, float|null $c = -0, float $d = true ? 3 : 4); }
class PdwfC { function __construct(public float $p = 5, float ...$rest) {} }
function pdwf_gen(float $g = 6, iterable|float $h = 7) { yield $g; yield $h; }
// Widened: the type lists float and not int. Kept: it lists int, or the
// default is a constant php never folds (only the bound argument converts).
function pdwf(float|string $s = 1, bool|float $b = 4, int|float $i = 3,
	int|float|string $v = 9, float $hex = 0x10, float $lit = 1.0, float $div = 4/2,
	float $k = PDWF_K, float $max = PHP_INT_MAX) { return func_get_args(); }
$af = fn(float $q = 1 <=> 2) => $q;
foreach ([['PdwfI','m'],['PdwfA','n'],['PdwfC','__construct']] as [$c,$m])
	foreach ((new ReflectionMethod($c,$m))->getParameters() as $p)
		if ($p->isDefaultValueAvailable()) var_dump($p->getDefaultValue());
foreach (['pdwf_gen', 'pdwf', $af] as $f)
	foreach ((new ReflectionFunction($f))->getParameters() as $p) var_dump($p->getDefaultValue());
var_dump(iterator_to_array(pdwf_gen()), $af(), (new PdwfC)->p, pdwf());
foreach (['pdwf', 'pdwf_gen'] as $f)
	foreach ((new ReflectionFunction($f))->getParameters() as $p) echo $p, "\n";
--EXPECT--
float(1)
float(2)
float(0)
float(3)
float(5)
float(6)
float(7)
float(1)
float(4)
int(3)
int(9)
float(16)
float(1)
float(2)
int(7)
int(9223372036854775807)
float(-1)
array(2) {
  [0]=>
  float(6)
  [1]=>
  float(7)
}
float(-1)
float(5)
array(0) {
}
Parameter #0 [ <optional> string|float $s = 1.0 ]
Parameter #1 [ <optional> float|bool $b = 4.0 ]
Parameter #2 [ <optional> int|float $i = 3 ]
Parameter #3 [ <optional> string|int|float $v = 9 ]
Parameter #4 [ <optional> float $hex = 16.0 ]
Parameter #5 [ <optional> float $lit = 1.0 ]
Parameter #6 [ <optional> float $div = 2.0 ]
Parameter #7 [ <optional> float $k = PDWF_K ]
Parameter #8 [ <optional> float $max = PHP_INT_MAX ]
Parameter #0 [ <optional> float $g = 6.0 ]
Parameter #1 [ <optional> Traversable|array|float $h = 7.0 ]
