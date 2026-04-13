--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: nullable parameters and null values
--FILE--
<?php
function nanlf(?string $a, ?int $b) {
    echo "a=" . (is_null($a) ? "NULL" : $a);
    echo " b=" . (is_null($b) ? "NULL" : $b) . "\n";
}
nanlf(a: null, b: null);
nanlf(b: 5, a: "hi");
nanlf(b: null, a: "x");
nanlf(a: null, b: 7);
?>
--EXPECT--
a=NULL b=NULL
a=hi b=5
a=x b=NULL
a=NULL b=7
--CLEAN--
<?php
