--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk with callback that returns false
--FILE--
<?php
function callback($value, $key) {
    echo "Key: $key, Value: $value\n";
    return false;
}
$array = array('a' => 1, 'b' => 2);
$result = array_walk($array, 'callback');
echo "Result: " . ($result ? "true" : "false") . "\n";
?>
--EXPECT--
Key: a, Value: 1
Key: b, Value: 2
Result: true
--CLEAN--
<?php
unset($array, $result);
