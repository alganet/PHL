--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill: values are inserted correctly
--FILE--
<?php
$a = array_fill(2, 2, 'z');
// verify keys and values manually to avoid var_dump differences
$keys = array_keys($a);
echo $keys[0] . "\n";  // first key should equal start index
echo $a[2] . "\n";      // first value
echo $a[3] . "\n";      // second value
?>
--EXPECT--
2
z
z
--CLEAN--
<?php
unset($a, $keys);
