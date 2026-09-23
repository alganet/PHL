--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A by-reference foreach writes through a property or element target, and only the last element stays a reference
--FILE--
<?php
class FeRefHolder {
    public $v;
}

$holder = new FeRefHolder;
$nums = [1, 2, 3];
foreach ($nums as &$holder->v) {
    $holder->v *= 10;
}
var_dump($nums);

$holder->v = 99;
echo $nums[2], "\n";

$slot = [];
$more = [1, 2];
foreach ($more as &$slot["cur"]) {
    $slot["cur"]++;
}
var_dump($more, $slot);
?>
--EXPECT--
array(3) {
  [0]=>
  int(10)
  [1]=>
  int(20)
  [2]=>
  &int(30)
}
99
array(2) {
  [0]=>
  int(2)
  [1]=>
  &int(3)
}
array(1) {
  ["cur"]=>
  &int(3)
}
--CLEAN--
<?php
unset($holder, $nums, $slot, $more);
