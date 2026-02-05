--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff_uassoc should use user callback for key comparison
--FILE--
<?php
$array1 = array('a' => 1, 'b' => 2, 'c' => 3);
$array2 = array('x' => 1, 'b' => 2, 'c' => 4);

// Simple key comparison - return 0 if keys are equal
$result = array_diff_uassoc($array1, $array2, function($a, $b) {
    return strcmp($a, $b);
});

echo count($result) . PHP_EOL;
foreach($result as $k => $v) {
    echo $k . ':' . $v . PHP_EOL;
}
?>
--EXPECT--
2
a:1
c:3
--CLEAN--
<?php
unset($array1, $array2, $result);
