--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A static closure is a property default, not a property-hook list
--FILE--
<?php
/* A static closure is a constant expression in php, so it may be a property
 * default. The scan that delimits a default ends it at the first depth-0 '{',
 * which is the php 8.4 hook-list opener -- and it read the CLOSURE's brace as
 * one, so the default was truncated at `static function()` and the declaration
 * died three errors deep. A class CONSTANT never showed it: only a property
 * carries a hook list at all. */
class PropDefaultClosure
{
    const K = 7;
    public $fn = static function () { return 1; };
    public $arr = static function () { return ['a' => 1]; };
    public $nested = static function () { return static function () { return 2; }; };
    public static $stat = static function () { return 3; };
    public $pair = [1, static function () { return 4; }];
    public $plain = 5, $after = 6;
    public string $hooked = 'init' { get => strtoupper($this->hooked); }
    public $hookedToo = 8 { get { return $this->hookedToo + 1; } }
}

$o = new PropDefaultClosure();
var_dump(($o->fn)());
var_dump(($o->arr)());
var_dump((($o->nested)())());
var_dump((PropDefaultClosure::$stat)());
var_dump($o->pair[0], ($o->pair[1])());
var_dump($o->plain, $o->after);
var_dump($o->hooked, $o->hookedToo);
?>
--EXPECT--
int(1)
array(1) {
  ["a"]=>
  int(1)
}
int(2)
int(3)
int(1)
int(4)
int(5)
int(6)
string(4) "INIT"
int(9)
--CLEAN--
<?php
