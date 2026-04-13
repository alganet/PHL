--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: pass by reference
--FILE--
<?php
function nabrf($a, &$b) {
    $b = $a * 2;
}
$val = 0;
nabrf(b: $val, a: 5);
echo "val=$val\n";
nabrf(a: 10, b: $val);
echo "val=$val\n";
?>
--EXPECT--
val=10
val=20
--CLEAN--
<?php
