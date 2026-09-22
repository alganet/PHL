--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a call dispatched from the engine reports the CALL SITE's line, deterministically
--FILE--
<?php
class C {
    public function __get($n) { return debug_backtrace()[0]['line']; }
    public function __call($n, $a) { return debug_backtrace()[0]['line']; }
    public function __toString() { throw new Exception('boom'); }
}

$c = new C;
var_dump($c->missing);
var_dump($c->missingMethod());
try { (string)$c; }
catch (Exception $e) { var_dump($e->getTrace()[0]['line']); }
?>
--EXPECT--
int(9)
int(10)
int(11)
--CLEAN--
<?php
