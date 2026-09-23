--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `new`'s arguments are arguments: the deferred ones resolve against the constructor
--DESCRIPTION--
An element/property argument whose target is absent at load time rides a deferred
carrier that only OP_CALL resolved, and a constructor is reached by pointer rather
than through that opcode. So `new C($a['missing'])` passed a silent NULL where php
warns about the read, and a by-REFERENCE constructor parameter never vivified its
target at all — php creates the element/property and the constructor writes it,
PHL left it uncreated. A class with NO constructor still reports what reading the
arguments found, because php evaluated them either way.
--FILE--
<?php
class NewArgRef { public function __construct(&$x) { $x = 'W'; } }
class NewArgVal { public function __construct($x) { var_dump($x); } }
class NewArgNone { }
class NewArgHost { public $obj; public $made; }
function newArgTry($label, $fn) {
    echo $label, ":\n";
    /* One interpreter for the whole corpus: register and restore per probe. */
    set_error_handler(function ($n, $m) { echo "  Warning: ", $m, "\n"; return true; });
    try { $fn(); } catch (Throwable $e) { echo "  ", get_class($e), ": ", $e->getMessage(), "\n"; }
    restore_error_handler();
}

/* By VALUE: php reads the argument and reports what it found. */
newArgTry('missing element', function () { $a = ['k' => 1]; new NewArgVal($a['missing']); });
newArgTry('missing property', function () { $o = new NewArgHost; new NewArgVal($o->nope); });
newArgTry('nested missing', function () { $a = ['n' => []]; new NewArgVal($a['n']['zz']); });
newArgTry('undefined variable', function () { new NewArgVal($newArgUndef); });
newArgTry('no constructor', function () { $a = []; new NewArgNone($a['gone']); });

/* By REFERENCE: php vivifies the target and the constructor writes through it. */
newArgTry('element vivified', function () {
    $a = []; new NewArgRef($a['fresh']); var_dump($a);
});
newArgTry('declared property', function () {
    $o = new NewArgHost; new NewArgRef($o->made); var_dump($o->made);
});
newArgTry('nested vivified', function () {
    $a = ['n' => []]; new NewArgRef($a['n']['deep']); var_dump($a['n']);
});
newArgTry('undefined variable', function () {
    new NewArgRef($newArgOut); var_dump($newArgOut);
});

/* A string OFFSET cannot be either end of a reference, and says so at the `new`. */
newArgTry('string offset', function () { $s = 'abc'; new NewArgRef($s[1]); });

/* An accessor that THROWS while the argument is read abandons the construction. */
class NewArgBoom { public function __get($n) { throw new RuntimeException('from-get'); } }
newArgTry('throwing accessor', function () { new NewArgVal((new NewArgBoom)->x); });
echo "end\n";
?>
--EXPECT--
missing element:
  Warning: Undefined array key "missing"
NULL
missing property:
  Warning: Undefined property: NewArgHost::$nope
NULL
nested missing:
  Warning: Undefined array key "zz"
NULL
undefined variable:
  Warning: Undefined variable $newArgUndef
NULL
no constructor:
  Warning: Undefined array key "gone"
element vivified:
array(1) {
  ["fresh"]=>
  string(1) "W"
}
declared property:
string(1) "W"
nested vivified:
array(1) {
  ["deep"]=>
  string(1) "W"
}
undefined variable:
string(1) "W"
string offset:
  Error: Cannot create references to/from string offsets
throwing accessor:
  RuntimeException: from-get
end
