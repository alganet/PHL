--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A readonly property from a trait is writable from the using class, incl. re-init in __clone
--DESCRIPTION--
Regression: a readonly property imported from a trait kept the TRAIT as its
declaring class, so the set-scope check (instanceof against a trait, which is
never in the hierarchy) rejected every write with "Cannot modify protected(set)
readonly property Trait::$x from scope C". Traits are flattened into the using
class, so the composing class is the set-scope. Also covers PHP 8.3 readonly
re-initialization inside __clone(). Outside those, the property stays locked.
--FILE--
<?php
trait State { public readonly int $n; public function set(int $v){ $this->n = $v; } }
trait Cloner { public function __clone(){ $this->n = $this->n * 10; } }
class RtacBox { use State; use Cloner; }
$a = new RtacBox(); $a->set(3);
$b = clone $a;
echo "a={$a->n} b={$b->n}\n";
try { $a->n = 99; } catch (\Error $e) { echo "locked: yes\n"; }
?>
--EXPECT--
a=3 b=30
locked: yes
--CLEAN--
<?php
