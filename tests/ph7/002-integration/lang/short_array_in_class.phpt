--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array syntax in class properties and method returns
--FILE--
<?php
class Config {
    public $items = array();
    public function getDefaults() {
        return ['a' => 1, 'b' => 2, 'c' => 3];
    }
    public function setItems($items) {
        $this->items = $items;
    }
}
$c = new Config();
$d = $c->getDefaults();
echo $d['a'], "\n";
echo $d['b'], "\n";

$c->setItems(['x', 'y', 'z']);
echo $c->items[0], "\n";
echo $c->items[2], "\n";

echo count(['one', 'two', 'three']), "\n";
?>
--EXPECT--
1
2
x
z
3
--CLEAN--
<?php
