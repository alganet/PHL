--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_class_vars() answers class defaults, own declarations before inherited ones
--FILE--
<?php
// php walks the property table twice -- every non-static, then every static --
// and reads a static out of the class's DEFAULT table, so a live assignment to
// the static before the call cannot be seen here. The table itself is built
// own-declarations-first and appended to by each inheritance step, so an
// inherited property always follows the listing class's own. A private belongs
// to the class that DECLARED it and to no subclass listing.
class P {
    public $pa = 'pa';
    public static $ps = 'ps';
    protected $pb = 'pb';
    protected static $pbs = 'pbs';
    private $ppriv = 'ppriv';
    private static $pprivs = 'pprivs';
}
class C extends P {
    public static $s = 'def';
    public $a = 1;
    protected $b = 2;
    public static $t = 'tt';
    public $c = 3;
    public int $typed;
    public ?string $tn = null;
    public static int $ts;
}
C::$s = 'LIVE';
P::$ps = 'LIVEPS';

echo "--- from outside\n";
var_dump(get_class_vars('C'));

class D extends P {
    static function look() {
        echo "--- P from a subclass scope\n";
        var_dump(get_class_vars('P'));
        echo "--- D from its own scope\n";
        var_dump(get_class_vars('D'));
    }
}
D::look();

class E {
    public $x = 1;
    private $y = 2;
    protected $z = 3;
    public static $sx = 4;
    private static $sy = 5;
    static function look() {
        echo "--- E from its own scope\n";
        var_dump(get_class_vars('E'));
    }
}
E::look();

echo "--- an interface has none\n";
interface I { const K = 1; }
var_dump(get_class_vars('I'));
?>
--EXPECT--
--- from outside
array(9) {
  ["a"]=>
  int(1)
  ["c"]=>
  int(3)
  ["typed"]=>
  NULL
  ["tn"]=>
  NULL
  ["pa"]=>
  string(2) "pa"
  ["s"]=>
  string(3) "def"
  ["t"]=>
  string(2) "tt"
  ["ts"]=>
  NULL
  ["ps"]=>
  string(2) "ps"
}
--- P from a subclass scope
array(4) {
  ["pa"]=>
  string(2) "pa"
  ["pb"]=>
  string(2) "pb"
  ["ps"]=>
  string(2) "ps"
  ["pbs"]=>
  string(3) "pbs"
}
--- D from its own scope
array(4) {
  ["pa"]=>
  string(2) "pa"
  ["pb"]=>
  string(2) "pb"
  ["ps"]=>
  string(2) "ps"
  ["pbs"]=>
  string(3) "pbs"
}
--- E from its own scope
array(5) {
  ["x"]=>
  int(1)
  ["y"]=>
  int(2)
  ["z"]=>
  int(3)
  ["sx"]=>
  int(4)
  ["sy"]=>
  int(5)
}
--- an interface has none
array(0) {
}
