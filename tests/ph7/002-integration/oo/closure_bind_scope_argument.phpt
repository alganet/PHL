--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A $newScope naming no class is a warning, decided before every other rebind rule
--FILE--
<?php
class CbsaBase {
    private $secret = 'hidden';
    private function priv() { return 'priv'; }
    public function mk() { return $this->priv(...); }
}
class CbsaOther {}
$o = new CbsaBase();
$other = new CbsaOther();

function cbsa($label, callable $make) {
    $c = $make();
    echo $label, ":", $c === null ? 'NULL' : $c(), "\n";
}

/* php resolves the scope argument FIRST: an unresolvable one wins over the receiver
 * refusals, and applies to a plain closure exactly as to a method callable. */
cbsa('method-bad-scope', fn() => $o->mk()->bindTo($o, 'CbsaNoSuchClass'));
cbsa('unrelated-and-bad-scope', fn() => $o->mk()->bindTo($other, 'CbsaNoSuchClass'));
cbsa('unbind-and-bad-scope', fn() => $o->mk()->bindTo(null, 'CbsaNoSuchClass'));
$plain = function () { return $this->secret; };
cbsa('plain-bad-scope', fn() => $plain->bindTo($o, 'CbsaNoSuchClass'));
/* `object|string|null` coerces a scalar in weak mode, so the number becomes the NAME. */
cbsa('scalar-scope', fn() => $plain->bindTo($o, 5));
/* A resolvable one still binds, by name or by object. */
cbsa('good-scope-name', fn() => $plain->bindTo($o, CbsaBase::class));
cbsa('good-scope-object', fn() => $plain->bindTo($o, $o));
$plain2 = function () { return 'kept'; };
cbsa('keep-scope', fn() => $plain2->bindTo($o, 'static'));
?>
--EXPECTF--
PHP Warning:  Class "CbsaNoSuchClass" not found in %s on line %d
method-bad-scope:NULL
PHP Warning:  Class "CbsaNoSuchClass" not found in %s on line %d
unrelated-and-bad-scope:NULL
PHP Warning:  Class "CbsaNoSuchClass" not found in %s on line %d
unbind-and-bad-scope:NULL
PHP Warning:  Class "CbsaNoSuchClass" not found in %s on line %d
plain-bad-scope:NULL
PHP Warning:  Class "5" not found in %s on line %d
scalar-scope:NULL
good-scope-name:hidden
good-scope-object:hidden
keep-scope:kept
--CLEAN--
<?php
