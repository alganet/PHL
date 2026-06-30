--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class-typed parameter mismatch throws a catchable TypeError (self/parent/interface/abstract enforced)
--FILE--
<?php
class PteA {}
class PteB extends PteA {}
class PteX {}
interface PteI {}
class PteImpl implements PteI {}
abstract class PteAbs {}
class PteAbsImpl extends PteAbs {}

/* Returns "TE" if calling $fn throws a TypeError whose message matches PHP's
 * wording, "no-throw" otherwise. The message tail ", called in <file> on line N"
 * that PHP appends is intentionally not asserted (PHL omits it for every
 * exception — a separate error-format-fidelity item). */
function pteCheck(callable $fn): string {
    try { $fn(); return "no-throw"; }
    catch (\TypeError $e) {
        return str_contains($e->getMessage(), "must be of type") ? "TE" : "TE?";
    }
}

/* concrete class param: exact + subclass accepted, mismatch throws */
function fc(PteA $x) { return get_class($x); }
echo fc(new PteA), "\n";                  // PteA
echo fc(new PteB), "\n";                  // PteB
echo pteCheck(fn() => fc("oops")), "\n";  // TE
echo pteCheck(fn() => fc(new PteX)), "\n";// TE

/* the message prefix is PHP-exact (sans the "called in" suffix) */
try { fc("oops"); } catch (\TypeError $e) {
    echo substr($e->getMessage(), 0, strpos($e->getMessage(), " given") + 6), "\n";
}

/* interface + abstract param (interface/abstract PARAM enforcement) */
function fi(PteI $x) { return "i-ok"; }
echo fi(new PteImpl), "\n";               // i-ok
echo pteCheck(fn() => fi(new PteX)), "\n";// TE
function fa(PteAbs $x) { return "a-ok"; }
echo fa(new PteAbsImpl), "\n";            // a-ok
echo pteCheck(fn() => fa(new PteX)), "\n";// TE

/* self / parent, including inherited (lexical) self */
class PteS { function m(self $x) { return "s-ok"; } }
class PteSsub extends PteS {}
$s = new PteS;
echo $s->m($s), "\n";                     // s-ok
echo $s->m(new PteSsub), "\n";            // s-ok (subclass)
echo pteCheck(fn() => $s->m("x")), "\n";  // TE
$sub = new PteSsub;
echo $sub->m(new PteS), "\n";             // s-ok (inherited self = PteS, base instance ok)

class PteP extends PteS { function p(parent $x) { return "p-ok"; } }
$p = new PteP;
echo $p->p(new PteS), "\n";               // p-ok
echo pteCheck(fn() => $p->p("x")), "\n";  // TE

/* named-argument call form hits the second binding site */
echo pteCheck(fn() => fc(x: "oops")), "\n"; // TE

/* trait method: self/parent resolve to the USING class (not the trait) */
trait PteT { function tm(self $x) { return "t-ok"; } }
class PteUser { use PteT; }
$u = new PteUser;
echo $u->tm($u), "\n";                    // t-ok
echo pteCheck(fn() => $u->tm("x")), "\n"; // TE (message qualifies "PteUser::tm")
trait PteTp { function tp(parent $x) { return "tp-ok"; } }
class PteParentUser extends PteA { use PteTp; }
echo (new PteParentUser)->tp(new PteA), "\n"; // tp-ok (parent = PteA, the using class's base)
?>
--EXPECT--
PteA
PteB
TE
TE
fc(): Argument #1 ($x) must be of type PteA, string given
i-ok
TE
a-ok
TE
s-ok
s-ok
TE
s-ok
p-ok
TE
TE
t-ok
TE
tp-ok
--CLEAN--
<?php
