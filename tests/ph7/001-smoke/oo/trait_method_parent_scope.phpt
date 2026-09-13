--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
parent:: inside a trait method resolves against the using class (method, const, __clone)
--FILE--
<?php
class TmpsBase { const K = 5; public function hi() { echo "base hi\n"; } public function __clone() { echo "base clone\n"; } }
trait T {
    public function hi() { echo "trait hi\n"; parent::hi(); }
    public function useConst() { return parent::K; }
    public function __clone() { echo "trait clone\n"; parent::__clone(); }
}
class TmpsChild extends TmpsBase { use T; }
$c = new TmpsChild;
$c->hi();
echo "const: " . $c->useConst() . "\n";
$d = clone $c;
?>
--EXPECT--
trait hi
base hi
const: 5
trait clone
base clone
--CLEAN--
<?php
unset($c, $d);
