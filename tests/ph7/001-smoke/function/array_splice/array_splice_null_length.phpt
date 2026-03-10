--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_splice with NULL length removes everything from offset to end
--FILE--
<?php
$a = array(1, 2, 3, 4, 5);
$r = array_splice($a, 2, NULL);
echo implode(',', $r) . "\n";
echo implode(',', $a);
?>
--EXPECT--
3,4,5
1,2
--CLEAN--
<?php
unset($a, $r);
