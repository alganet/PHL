--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array key containing a leading zero should be treated as a string (not an integer)
--FILE--
<?php
$a = array();
$a['01'] = 'string-key';
$a[1] = 'int-key';
// We expect two entries because '01' should be a string key, not auto-cast to int
echo count($a) . PHP_EOL;
// Ensure that the keys keep their types, '01' is string and 1 remains int
$keys = array_keys($a);
foreach($keys as $k){
    echo (is_int($k) ? 'int:' : 'string:') . $k . PHP_EOL;
}
?>
--EXPECT--
2
string:01
int:1
--CLEAN--
<?php
unset($a, $keys);
