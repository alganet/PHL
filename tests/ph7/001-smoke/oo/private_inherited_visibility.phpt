--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A base's private property is INVISIBLE below it, not merely inaccessible
--DESCRIPTION--
Because php stores a private property under its MANGLED name, a lookup by the
plain one finds nothing at all anywhere below the declaring class: `$b->q` is
"Undefined property: B::$q" and an unset() of it a silent no-op, where the
DECLARING class's own instance gets the visibility refusal. A subclass's own scope
is outside it too, and the name is on none of the subclass's listing surfaces.
This engine answered "Cannot access private property" to every one of them.

The one face left out is the WRITE, which php answers with dynamic-property
creation behind a deprecation; the non-deprecated policy makes it an Error here, twinned
in 002-integration/oo/private_inherited_dynamic_write{,_zend}.phpt.
--FILE--
<?php
set_error_handler(function ($n, $m) { echo "[warn] $m\n"; return true; });
class PrivVisBase { private $q = 1; }
class PrivVisMid extends PrivVisBase {
    public function midRead()  { return $this->q; }
    public function midIsset() { return isset($this->q); }
    public function midUnset() { unset($this->q); return 'unset returned'; }
}
class PrivVisGrand extends PrivVisMid {}

function priv_vis_try($tag, callable $fn) {
    echo $tag, ': ';
    try { var_dump($fn()); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}

priv_vis_try('child read   ', fn() => (new PrivVisMid)->q);
priv_vis_try('grand read   ', fn() => (new PrivVisGrand)->q);
priv_vis_try('own read     ', fn() => (new PrivVisBase)->q);
priv_vis_try('child isset  ', fn() => isset((new PrivVisMid)->q));
priv_vis_try('child coalesc', fn() => (new PrivVisMid)->q ?? 'default');
$pv = new PrivVisMid;
priv_vis_try('child unset  ', function() use ($pv) { unset($pv->q); return 'silent'; });
priv_vis_try('still there  ', fn() => str_replace("\0", '@', serialize($pv)));

$pvm = new PrivVisMid;
priv_vis_try('scope read   ', fn() => $pvm->midRead());
priv_vis_try('scope isset  ', fn() => $pvm->midIsset());
priv_vis_try('scope unset  ', fn() => $pvm->midUnset());
priv_vis_try('scope kept   ', fn() => str_replace("\0", '@', serialize($pvm)));

var_dump(property_exists('PrivVisMid', 'q'));
var_dump(property_exists('PrivVisBase', 'q'));
var_dump((new ReflectionClass('PrivVisMid'))->hasProperty('q'));
var_dump(get_class_vars('PrivVisMid'));
priv_vis_try('reflect mid  ', fn() => (new ReflectionProperty('PrivVisMid', 'q'))->getName());
priv_vis_try('reflect base ', fn() => (new ReflectionProperty('PrivVisBase', 'q'))->getName());
restore_error_handler();
?>
--EXPECT--
child read   : [warn] Undefined property: PrivVisMid::$q
NULL
grand read   : [warn] Undefined property: PrivVisGrand::$q
NULL
own read     : Error: Cannot access private property PrivVisBase::$q
child isset  : bool(false)
child coalesc: string(7) "default"
child unset  : string(6) "silent"
still there  : string(48) "O:10:"PrivVisMid":1:{s:14:"@PrivVisBase@q";i:1;}"
scope read   : [warn] Undefined property: PrivVisMid::$q
NULL
scope isset  : bool(false)
scope unset  : string(14) "unset returned"
scope kept   : string(48) "O:10:"PrivVisMid":1:{s:14:"@PrivVisBase@q";i:1;}"
bool(false)
bool(true)
bool(false)
array(0) {
}
reflect mid  : ReflectionException: Property PrivVisMid::$q does not exist
reflect base : string(1) "q"
--CLEAN--
<?php
unset($pv, $pvm);
