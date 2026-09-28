--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
isset() with several operands is a short-circuit chain: nothing after the first miss runs
--FILE--
<?php
set_error_handler(function ($n, $s) { echo "ERR[$n] $s\n"; return true; });

function issetChainSide($tag) { echo "side($tag)\n"; return 'k'; }

$issetChainInfo = ['a' => 1];
$issetChainData = [];

/* A later operand is never EVALUATED once an earlier one is missing: no
 * "Undefined array key", no null-offset deprecation, no call. */
var_dump(isset($issetChainInfo['nope'], $issetChainData[$issetChainInfo['nope']]));
var_dump(isset($issetChainInfo['nope'], $issetChainData[issetChainSide('a')]));
/* …and it IS evaluated once every operand before it is set. */
var_dump(isset($issetChainInfo['a'], $issetChainData[issetChainSide('b')]));

/* The miss can sit anywhere in the chain. */
var_dump(isset($issetChainInfo['a'], $issetChainInfo['zz'], $issetChainData[issetChainSide('c')]));
var_dump(isset($issetChainInfo['a'], $issetChainInfo['a'], $issetChainInfo['a']));

/* A null operand is a miss too, and stops the chain. */
$issetChainNull = null;
var_dump(isset($issetChainNull, $issetChainData[issetChainSide('d')]));

/* ArrayAccess: only the operands the chain reaches are asked. */
class IssetChainAccess implements ArrayAccess
{
	private $d = ['a' => 1, 'n' => null];
	public function offsetExists($o): bool { echo "exists($o)\n"; return isset($this->d[$o]); }
	public function offsetGet($o): mixed { return $this->d[$o] ?? null; }
	public function offsetSet($o, $v): void {}
	public function offsetUnset($o): void {}
}
$issetChainAa = new IssetChainAccess();
var_dump(isset($issetChainAa['n'], $issetChainAa['a']));
var_dump(isset($issetChainAa['a'], $issetChainAa['n']));

/* __isset the same way. */
class IssetChainMagic
{
	private $p = ['x' => 1];
	public function __isset($n) { echo "__isset($n)\n"; return isset($this->p[$n]); }
	public function __get($n) { return $this->p[$n] ?? null; }
}
$issetChainM = new IssetChainMagic();
var_dump(isset($issetChainM->y, $issetChainM->x));
var_dump(isset($issetChainM->x, $issetChainM->y));

/* An access CHAIN inside one operand keeps its own silent intermediates. */
$issetChainArr = ['k' => ['j' => 2]];
var_dump(isset($issetChainArr['k'], $issetChainArr['k']['j'], $issetChainArr['q']['z']));

/* Uninitialised typed static, string offsets, and the single-operand form. */
class IssetChainStatics { public static $s = 1; public static ?int $t; }
var_dump(isset(IssetChainStatics::$t, IssetChainStatics::$s));
var_dump(isset(IssetChainStatics::$s, IssetChainStatics::$t));
$issetChainStr = 'abc';
var_dump(isset($issetChainStr[9], $issetChainStr[0]));
var_dump(isset($issetChainStr[0], $issetChainStr[9]));
var_dump(isset($issetChainArr['k']));

/* The keyword is case-insensitive, and a METHOD named `isset` is untouched. */
var_dump(ISSET($issetChainArr['k'], $issetChainArr['zz']));
class IssetChainHost { public function isset(...$a) { return 'method:' . count($a); } }
var_dump((new IssetChainHost())->isset(1, 2, 3));

restore_error_handler();
?>
--EXPECT--
bool(false)
bool(false)
side(b)
bool(false)
bool(false)
bool(true)
bool(false)
exists(n)
bool(false)
exists(a)
exists(n)
bool(false)
__isset(y)
bool(false)
__isset(x)
__isset(y)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(true)
bool(false)
string(8) "method:3"
