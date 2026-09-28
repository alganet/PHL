--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Which shutdown pass an object falls into, by what is holding it
--DESCRIPTION--
The rule that decides between php's two shutdown passes, exercised through every way a
program can still be holding an object at the end.

The symbol-table pass takes only an entry php sees as `IS_OBJECT` with a refcount of 1 --
one memory object holds the instance, and one NAME holds that -- and it walks backwards,
so `$last` goes before `$holder` goes before `$plain`. Freeing `$holder` there takes its
property with it, in that same pass, before `$plain` is even reached.

Everything else waits for the object-store pass and goes in CREATION order: a name written
with `&` (php's `IS_REFERENCE`, which is not an object zval at all), an object two names
copy, one only an array holds, one inside a cycle, a class static and a function static
(neither is in the symbol table to begin with). An object dropped mid-script is destructed
right there and takes no part in either.
--FILE--
<?php
class Mark
{
    public $n;
    public $o;
    public function __construct($n) { $this->n = $n; }
    public function __destruct() { echo "D:{$this->n}\n"; }
}

class Slot { public static $kept; }

function keep($v)
{
    static $s;
    if ($s === null) {
        $s = $v;
    }
}

$plain     = new Mark('plain');
$aliased   = new Mark('by-reference');
$alias     = &$aliased;
$copy      = new Mark('two-names');
$second    = $copy;
$holder    = new Mark('holder');
$holder->o = new Mark('held-by-a-property');
$cycle     = new Mark('cycle');
$cycle->o  = $cycle;
$nested    = ['a' => ['b' => new Mark('nested-array')]];
Slot::$kept = new Mark('class-static');
keep(new Mark('function-static'));
$gone = new Mark('unset-before-the-end');
unset($gone);
$last = new Mark('last');
echo "script-end\n";
?>
--EXPECT--
D:unset-before-the-end
script-end
D:last
D:holder
D:held-by-a-property
D:plain
D:by-reference
D:two-names
D:cycle
D:nested-array
D:class-static
D:function-static
