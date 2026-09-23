--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A by-REFERENCE argument naming a property of a non-object is php's Error
--DESCRIPTION--
Asking a null/int/string/bool for something to MODIFY is php's catchable Error, and every
other write shape through such a base already raised it; `f($u->p)` with `function f(&$x)`
stayed the READ warning and handed the parameter a NULL, so the callee's write went
nowhere on a statement php stops the script for. The by-VALUE binding really is the read
warning, so which diagnostic applies is the callee's to decide — the member op records the
step and OP_CALL re-drives it in the mode the resolved callee chose, exactly as it already
does for a property MISSING on a real object.
--FILE--
<?php
function bnoRef(&$x) { $x = 'W'; }
function bnoVal($x) { var_dump($x); }
function bnoTry($label, $fn) {
    echo $label, ":\n";
    /* Normalize the warning line so the two engines' diagnostic SHAPES (file and line
     * tail) stay out of it; the message body and the order are what this pins.
     * Registered per probe and restored, because this corpus shares one interpreter. */
    set_error_handler(function ($n, $m) { echo "  Warning: ", $m, "\n"; return true; });
    try { $fn(); } catch (Throwable $e) { echo "  ", get_class($e), ": ", $e->getMessage(), "\n"; }
    restore_error_handler();
}
class BnoHost { public $q; public $obj; }

/* By reference: php's Error, and the base is left alone. */
bnoTry('null base',   function () { $u = null;  bnoRef($u->p); var_dump($u); });
bnoTry('int base',    function () { $i = 5;     bnoRef($i->p); var_dump($i); });
bnoTry('string base', function () { $z = 'zz';  bnoRef($z->p); var_dump($z); });
bnoTry('bool base',   function () { $b = false; bnoRef($b->p); var_dump($b); });
bnoTry('null middle', function () { $o = new BnoHost; bnoRef($o->obj->deep); var_dump($o->obj); });
bnoTry('builtin',     function () { $u = null; sort($u->p); });

/* By value: php's read warning and a NULL, which is what it always was. */
bnoTry('null byval',  function () { $u = null; bnoVal($u->p); var_dump($u); });
bnoTry('int byval',   function () { $i = 5;    bnoVal($i->p); });
bnoTry('middle byval', function () { $o = new BnoHost; bnoVal($o->obj->deep); });

/* A real property still binds and is still written through. */
bnoTry('real prop',   function () { $o = new BnoHost; bnoRef($o->q); var_dump($o->q); });
echo "end\n";
?>
--EXPECT--
null base:
  Error: Attempt to modify property "p" on null
int base:
  Error: Attempt to modify property "p" on int
string base:
  Error: Attempt to modify property "p" on string
bool base:
  Error: Attempt to modify property "p" on false
null middle:
  Error: Attempt to modify property "deep" on null
builtin:
  Error: Attempt to modify property "p" on null
null byval:
  Warning: Attempt to read property "p" on null
NULL
NULL
int byval:
  Warning: Attempt to read property "p" on int
NULL
middle byval:
  Warning: Attempt to read property "deep" on null
NULL
real prop:
string(1) "W"
end
