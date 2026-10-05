--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_column() reads an object row the way a property read does, magic accessors included
--DESCRIPTION--
php asks an object row two questions before it reads a column from it: does a property of that
name exist, visible from the calling scope and initialized (null counts, no magic is consulted),
and failing that, does __isset say it is set. Only on a yes does it read -- the slot, or __get, or
the plain read's warning or Error. PHL looked the name up in the object's slot table alone, so a
column served by __get was dropped, a private or protected property was handed out from outside
its class, and an uninitialized typed property read as null. The same questions answer the
$index_key column, and a throw from either accessor leaves the call.
--FILE--
<?php
class ColPlain {
    public $a = 1; public $n = null; private $priv = 'p'; protected $prot = 'q';
    public int $typed; public $k = 'K';
    static function inside($rows) { return array_column($rows, 'priv', 'prot'); }
}
class ColMagic {
    public $real = 'r'; private $hid = 'h';
    function __isset($n) { echo "  isset($n)\n"; return $n !== 'no'; }
    function __get($n) { echo "  get($n)\n"; return "G$n"; }
}
class ColIssetOnly { function __isset($n) { echo "  isset($n)\n"; return true; } }
class ColGetOnly { function __get($n) { echo "  get($n)\n"; return 7; } }
class ColPrivIsset { private $p = 1; function __isset($n) { return true; } }
class ColTypedMagic {
    public int $t;
    function __isset($n) { echo "  isset($n)\n"; return true; }
    function __get($n) { return 'G'; }
}
class ColUnset {
    public $x = 1;
    function __construct() { unset($this->x); }
    function __isset($n) { echo "  isset($n)\n"; return true; }
    function __get($n) { echo "  get($n)\n"; return 'viaGet'; }
}
class ColRecursive {
    function __isset($n) { echo "  isset($n)\n"; return true; }
    function __get($n) { echo "  get($n)\n"; return array_column([$this], $n); }
}
class ColBase { private $bp = 'base'; function col($r) { return array_column($r, 'bp'); } }
class ColKid extends ColBase {}
class ColIssetThrows { function __isset($n) { throw new Exception("isset threw"); } }
class ColGetThrows {
    function __isset($n) { return true; }
    function __get($n) { throw new Exception("get threw"); }
}
class ColTrace {
    function __isset($n) { return true; }
    function __get($n) {
        foreach (array_slice(debug_backtrace(), 0, 2) as $f) echo '  ', $f['function'], '@', $f['line'] ?? '-', "\n";
        return 1;
    }
}
set_error_handler(function ($no, $msg) { echo "  [$no] $msg\n"; return true; });

$cases = [
    'public' => fn() => array_column([new ColPlain], 'a'),
    'null public' => fn() => array_column([new ColPlain], 'n'),
    'private from outside' => fn() => array_column([new ColPlain], 'priv'),
    'protected from outside' => fn() => array_column([new ColPlain], 'prot'),
    'private from inside, keyed by protected' => fn() => ColPlain::inside([new ColPlain]),
    'uninitialized typed' => fn() => array_column([new ColPlain], 'typed'),
    'uninitialized typed beside __isset' => fn() => array_column([new ColTypedMagic], 't'),
    'magic' => fn() => array_column([new ColMagic], 'x'),
    'magic says no' => fn() => array_column([new ColMagic], 'no'),
    'magic class, public property' => fn() => array_column([new ColMagic], 'real'),
    'magic class, private property' => fn() => array_column([new ColMagic], 'hid'),
    'magic index' => fn() => array_column([new ColMagic], 'real', 'y'),
    'magic index, whole row' => fn() => array_column([new ColMagic], null, 'z'),
    'integer column name' => fn() => array_column([new ColMagic], 5),
    '__isset alone' => fn() => array_column([new ColIssetOnly], 'x'),
    '__get alone' => fn() => array_column([new ColGetOnly], 'x'),
    '__isset over an inaccessible property, no __get' => fn() => array_column([new ColPrivIsset], 'p'),
    'unset declared property' => fn() => array_column([new ColUnset], 'x'),
    'inside its own __get' => fn() => array_column([new ColRecursive], 'q'),
    'inherited private, from the declaring class' => fn() => (new ColBase)->col([new ColKid]),
    'index keyed on two public properties' => fn() => array_column([new ColPlain, new ColPlain], 'a', 'k'),
    '__isset throws' => fn() => array_column([new ColIssetThrows], 'x'),
    '__get throws' => fn() => array_column([new ColGetThrows], 'x'),
    'trace inside __get' => fn() => array_column([new ColTrace], 'x'),
];
foreach ($cases as $name => $f) {
    echo "$name:\n";
    try {
        $r = $f();
        echo '  ', json_encode($r), "\n";
    } catch (Throwable $e) {
        echo '  ', get_class($e), ': ', $e->getMessage(), "\n";
    }
}
?>
--EXPECT--
public:
  [1]
null public:
  [null]
private from outside:
  []
protected from outside:
  []
private from inside, keyed by protected:
  {"q":"p"}
uninitialized typed:
  []
uninitialized typed beside __isset:
  []
magic:
  isset(x)
  get(x)
  ["Gx"]
magic says no:
  isset(no)
  []
magic class, public property:
  ["r"]
magic class, private property:
  isset(hid)
  get(hid)
  ["Ghid"]
magic index:
  isset(y)
  get(y)
  {"Gy":"r"}
magic index, whole row:
  isset(z)
  get(z)
  {"Gz":{"real":"r"}}
integer column name:
  isset(5)
  get(5)
  ["G5"]
__isset alone:
  isset(x)
  [2] Undefined property: ColIssetOnly::$x
  [null]
__get alone:
  []
__isset over an inaccessible property, no __get:
  Error: Cannot access private property ColPrivIsset::$p
unset declared property:
  isset(x)
  get(x)
  ["viaGet"]
inside its own __get:
  isset(q)
  get(q)
  isset(q)
  [2] Undefined property: ColRecursive::$q
  [[null]]
inherited private, from the declaring class:
  ["base"]
index keyed on two public properties:
  {"K":1}
__isset throws:
  Exception: isset threw
__get throws:
  Exception: get threw
trace inside __get:
  __get@-
  array_column@70
  [1]
