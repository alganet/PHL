--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strict_types reaches a call the ENGINE dispatches: magic methods and property hooks
--FILE--
<?php
declare(strict_types=1);

class C {
    public $q;
    public int $p { set { $this->q = $value; } }
    public function __set(string $n, int $v) { $this->q = $v; }
    public function m(int $v) { return $v; }
}

$c = new C;

// A directly compiled call already honoured the file's mode.
try { $c->m("7"); echo "method: coerced\n"; }
catch (TypeError $e) { echo "method: throws\n"; }

// These reach the callee through a synthetic OP_CALL the engine builds, which
// carries no compiled call map — the mode has to come from the executing unit.
try { $c->p = "7"; echo "hook: coerced\n"; }
catch (TypeError $e) { echo "hook: throws\n"; }

try { $c->undeclared = "7"; echo "__set: coerced\n"; }
catch (TypeError $e) { echo "__set: throws\n"; }

var_dump($c->q);

// A WEAK file keeps weak binding even when a strict file called into it.
require __DIR__ . '/strict_types_through_engine_dispatch.inc';
weakDispatch("7");
?>
--EXPECT--
method: throws
hook: throws
__set: throws
NULL
int(7)
--CLEAN--
<?php
