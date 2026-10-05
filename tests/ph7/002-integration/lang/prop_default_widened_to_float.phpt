--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: an int default let into a float is stored as a float
--FILE--
<?php
declare(strict_types=1);
const PPWF_K = 7;
trait PpwfT { public float $t = 8; }
// Widened: the type lists float and not int. Kept: it lists int, or the
// default is a constant php never folds (until the class is resolved).
class PpwfC {
	use PpwfT;
	public float $a = 1;
	public ?float $b = 2;
	public float|string $s = 3;
	public bool|float $bf = 4;
	public int|float $i = 5;
	public int|float|string $v = 9;
	public float $hex = 0x10;
	public float $lit = 1.0;
	public float $div = 4/2;
	public float $cmp = 1 <=> 2;
	public float $neg = -6;
	public readonly float $ro;
	protected static float $st = 10;
	public float $k = PPWF_K;
	function __construct() { $this->ro = 1; }
	static function st() { return self::$st; }
}
foreach ((new ReflectionClass('PpwfC'))->getProperties() as $p)
	if ($p->hasDefaultValue()) var_dump($p->getName(), $p->getDefaultValue());
foreach (['a','b','s','i','hex','cmp','t','st'] as $n) echo new ReflectionProperty('PpwfC', $n);
$o = new PpwfC;
var_dump($o->a, $o->bf, $o->i, $o->t, $o->k, PpwfC::st());
--EXPECT--
string(1) "a"
float(1)
string(1) "b"
float(2)
string(1) "s"
float(3)
string(2) "bf"
float(4)
string(1) "i"
int(5)
string(1) "v"
int(9)
string(3) "hex"
float(16)
string(3) "lit"
float(1)
string(3) "div"
float(2)
string(3) "cmp"
float(-1)
string(3) "neg"
float(-6)
string(2) "st"
float(10)
string(1) "k"
int(7)
string(1) "t"
float(8)
Property [ public float $a = 1.0 ]
Property [ public ?float $b = 2.0 ]
Property [ public string|float $s = 3.0 ]
Property [ public int|float $i = 5 ]
Property [ public float $hex = 16.0 ]
Property [ public float $cmp = -1.0 ]
Property [ public float $t = 8.0 ]
Property [ protected static float $st = 10.0 ]
float(1)
float(4)
int(5)
float(8)
float(7)
float(10)
