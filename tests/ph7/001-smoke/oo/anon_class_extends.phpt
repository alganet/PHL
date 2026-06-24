--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Anonymous class: extends a base class
--FILE--
<?php
class AnonBaseA { protected $b = 3; function g() { return $this->b + 4; } }
$o = new class extends AnonBaseA {};
echo $o->g(), " ", var_export($o instanceof AnonBaseA, true), "\n";
?>
--EXPECT--
7 true
--CLEAN--
<?php
unset($o);
