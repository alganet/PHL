--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Rebinding a static property by reference gives its previous element back
--FILE--
<?php
class StatRebind {
    public static $s;
}

$a = [1, 2, 3];
StatRebind::$s =& $a[0];
StatRebind::$s =& $a[1];
StatRebind::$s =& $a[2];
var_dump($a);

$copy = $a;
$copy[0] = 99;
echo $a[0], " ", $copy[0], "\n";

StatRebind::$s = 30;
echo $a[2], "\n";
?>
--EXPECT--
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  &int(3)
}
1 99
30
--CLEAN--
<?php
unset($a, $copy);
