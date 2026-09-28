--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: a restated interface method must be compatible with the parent's (PHL half of the twin pair)
--DESCRIPTION--
php checks a restated interface method exactly as it checks an overriding class
method. PHL checked NEITHER parent -- an incompatible restatement of the first one
compiled in silence, and one of a later parent was refused for the wrong reason
("Cannot redeclare"), which is the declaration php ACCEPTS when the signatures agree.

Twinned because php prints the two SIGNATURES in this sentence and PHL prints the names
alone -- a wording gap this engine has for classes too, recorded in ECOSYSTEM.md as F36
together with the two overrides it wrongly accepts. The verdict and the line match; only
the rendering of the declaration does not.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
interface IriA { public function f(): string; }
interface IriS { public function g(): string; }
interface IriB extends IriA, IriS { public function g(): int; }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Declaration of IriB::g() must be compatible with IriS::g() %s
--CLEAN--
<?php
