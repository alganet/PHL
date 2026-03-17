--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge_recursive() appends values with numeric keys
--FILE--
<?php
$b = array_merge_recursive(array(1, 2), array(3));
echo $b[0] . "\n";
echo $b[1] . "\n";
echo $b[2] . "\n";
?>
--EXPECT--
1
2
3
--CLEAN--
<?php
unset($b);
