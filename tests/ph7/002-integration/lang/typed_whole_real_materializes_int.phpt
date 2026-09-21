--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A whole-valued float entering an int-typed context becomes a genuine int
--FILE--
<?php
// php's weak mode converts a lossless whole float (1.0) to a genuine int at
// every int-typed boundary; a float union member wins over int for a float
// value; a plain float context keeps the float. PHL's dual-flagged
// whole-real (the float-identity model) must materialize at each site.
const WR_FLOAT = 2.0;
function wrParam(int $a) { var_dump($a); }
function wrDefault(int $x = WR_FLOAT) { var_dump($x); }
function wrGenDefault(int $a, int $b = WR_FLOAT) { yield $b; }
function wrUnionIS(int|string $a) { var_dump($a); }
function wrUnionIF(int|float $a) { var_dump($a); }
function wrRet(): int { return 1.0; }
function wrVariadic(int ...$a) { var_dump($a); }
function wrGen(int $a) { yield $a; }
function wrFloat(float $a) { var_dump($a); }
class WrProp { public int $p; }

wrParam(1.0);
wrParam(4/2);        // 4/2 IS an int in both engines; must stay int(2)
wrParam(pow(2,3));   // php int(8); PHL's whole-real float(8) materializes
wrDefault();         // const-indirected whole-real default -> int(2)
foreach (wrGenDefault(1) as $v) { var_dump($v); }
wrUnionIS(1.0);      // int member takes the whole float -> int(1)
wrUnionIF(1.0);      // float member is an exact match -> stays float
var_dump(wrRet());
wrVariadic(1.0, x: 2.0);
foreach (wrGen(1.0) as $v) { var_dump($v); }
wrFloat(1.0);        // plain float context keeps the float
$o = new WrProp;
$o->p = 1.0;
var_dump($o->p);
$o->p = 6/3;
var_dump($o->p);
echo "ok\n";
?>
--EXPECT--
int(1)
int(2)
int(8)
int(2)
int(2)
int(1)
float(1)
int(1)
array(2) {
  [0]=>
  int(1)
  ["x"]=>
  int(2)
}
int(1)
float(1)
int(1)
int(2)
ok
--CLEAN--
<?php
