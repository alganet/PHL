--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A foreach target is assigned like any store: value before key, once per step
--FILE--
<?php
class FeTgtMagic {
    public function __set($name, $value) {
        echo "set($name=", var_export($value, true), ")\n";
    }
}

$m = new FeTgtMagic;
foreach (["a" => 1, "b" => 2] as $m->key => $m->value) {
}

function fetgt_next() {
    static $i = 0;
    echo "index\n";
    return $i++;
}

$out = [];
foreach ([10, 20] as $out[fetgt_next()]) {
}
var_dump($out);
?>
--EXPECT--
set(value=1)
set(key='a')
set(value=2)
set(key='b')
index
index
array(2) {
  [0]=>
  int(10)
  [1]=>
  int(20)
}
--CLEAN--
<?php
unset($m, $out);
