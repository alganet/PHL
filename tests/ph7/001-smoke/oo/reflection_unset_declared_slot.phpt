--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Reflection reads and writes a DECLARED property the object no longer holds
--DESCRIPTION--
Found while giving a base's private its own slot: a ReflectionProperty over a
property that unset() removed answered NULL in silence and wrote nowhere at all.
php reads it the way `$o->p` does -- the undefined-property warning and null, or
the uninitialized Error for a typed one -- and its write RE-CREATES the slot,
exactly as `$o->p = v` does.
--FILE--
<?php
set_error_handler(function ($n, $m) { echo "[warn] $m\n"; return true; });
class ReflUnsetSlot {
    public $p = 1;
    private $q = 2;
    public int $t = 3;
    public function drop() { unset($this->p, $this->q, $this->t); }
}
$rus = new ReflUnsetSlot;
$rus->drop();
echo str_replace("\0", '@', serialize($rus)), "\n";

/* A DECLARED property the object no longer holds reads the way `$o->p` does --
 * php's undefined-property warning and null -- and a write RE-CREATES the slot,
 * exactly as `$o->p = v` does. This engine answered null in silence and wrote
 * nowhere at all. */
$rp = new ReflectionProperty('ReflUnsetSlot', 'p');
var_dump($rp->isInitialized($rus));
var_dump($rp->getValue($rus));
$rp->setValue($rus, 9);
var_dump($rp->getValue($rus), $rus->p);

$rq = new ReflectionProperty('ReflUnsetSlot', 'q');
var_dump($rq->getValue($rus));
$rq->setValue($rus, 8);
var_dump($rq->getValue($rus));

/* A TYPED one comes back uninitialized, so the read is php's Error and not the
 * warning -- and the write settles it. */
$rt = new ReflectionProperty('ReflUnsetSlot', 't');
var_dump($rt->isInitialized($rus));
try { var_dump($rt->getValue($rus)); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
$rt->setValue($rus, 7);
var_dump($rt->isInitialized($rus), $rt->getValue($rus));
/* (the re-created slots' relative ORDER is the recorded VmRecreateDeclaredAttr
 * residual -- a property re-added after unset() appends here and keeps its
 * declared position in php -- so this reads the values rather than the payload) */
$rusKeys = array_keys(get_object_vars($rus)); sort($rusKeys);
var_dump($rus->p, $rus->t, $rusKeys);
restore_error_handler();
?>
--EXPECT--
O:13:"ReflUnsetSlot":0:{}
bool(false)
[warn] Undefined property: ReflUnsetSlot::$p
NULL
int(9)
int(9)
[warn] Undefined property: ReflUnsetSlot::$q
NULL
int(8)
bool(false)
Error: Typed property ReflUnsetSlot::$t must not be accessed before initialization
bool(true)
int(7)
int(9)
int(7)
array(2) {
  [0]=>
  string(1) "p"
  [1]=>
  string(1) "t"
}
--CLEAN--
<?php
unset($rus, $rp, $rq, $rt, $rusKeys);
