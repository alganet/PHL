--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: mutated via setter method
--FILE--
<?php
class TpCell {
    private int $value = 0;
    public function set(int $v): void { $this->value = $v; }
    public function get(): int { return $this->value; }
}
$c = new TpCell();
echo $c->get(), "\n";
$c->set(7);
echo $c->get(), "\n";
$c->set($c->get() * 3);
echo $c->get(), "\n";
?>
--EXPECT--
0
7
21
--CLEAN--
<?php
unset($c);
