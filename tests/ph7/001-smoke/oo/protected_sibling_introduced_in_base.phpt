--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a protected member introduced in a common base is reachable from a sibling scope even when overridden in the target
--FILE--
<?php
/* php checks protected access against the class that INTRODUCES the member, not
 * the one that re-declares the override that was resolved. So a sibling scope S
 * (extends B) may touch a protected member declared in B and overridden in a
 * sibling C (also extends B). PHL checked against C's override class and denied
 * it — the PHPUnit Constraint\Operator failure-description path
 * (`$constraint->failureDescription($other)` between sibling constraints).
 * A member declared ONLY in the target (not the shared base) stays denied. */
abstract class NpsBase {
    protected function shared(mixed $o): string { return "base:" . $o; }
    protected $tag = "B";
}
class NpsChild extends NpsBase {
    protected function shared(mixed $o): string { return "child:" . $o; } // override
    protected $tag = "C";
    protected function childOnly(): string { return "co"; }                // NOT in the base
}
class NpsSibling extends NpsBase {
    public function __construct(private NpsBase $c) {}
    public function callShared(mixed $o): string { $x = $this->c; return $x->shared($o); }
    public function readTag(): string { $x = $this->c; return $x->tag; }
    public function callChildOnly(): string {
        $x = $this->c;
        return $x instanceof NpsChild ? $x->childOnly() : "n/a";
    }
}
$s = new NpsSibling(new NpsChild());
echo $s->callShared(1), "\n";   // introduced in base, overridden in child -> child:1
echo $s->readTag(), "\n";        // protected prop introduced in base -> C
/* childOnly() is declared only in NpsChild, not the shared base: a sibling scope
 * has no access, which is a catchable Error. */
try { echo $s->callChildOnly(), "\n"; }
catch (\Error $e) { echo "denied\n"; }
?>
--EXPECT--
child:1
C
denied
--CLEAN--
<?php
