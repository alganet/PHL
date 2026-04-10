--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: auto-captures $this inside a method
--FILE--
<?php
class AfImBox {
    public $value = 42;
    public function make() {
        return fn($x) => $this->value + $x;
    }
}
$box = new AfImBox();
$f = $box->make();
echo $f(8), "\n";
?>
--EXPECT--
50
--CLEAN--
<?php
unset($box, $f);
