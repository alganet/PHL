--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_map accepts an object with __invoke as its callback
--FILE--
<?php
class Doubler {
    public function __invoke($x) {
        return $x * 2;
    }
}
class WithKey {
    private string $prefix;
    public function __construct(string $prefix) { $this->prefix = $prefix; }
    public function __invoke($x) {
        return $this->prefix . ":" . $x;
    }
}
echo implode(",", array_map(new Doubler(), [1, 2, 3, 4])), "\n";
echo implode(",", array_map(new WithKey("v"), [10, 20, 30])), "\n";
?>
--EXPECT--
2,4,6,8
v:10,v:20,v:30
--CLEAN--
<?php
