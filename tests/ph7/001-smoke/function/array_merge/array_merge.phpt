--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: array_merge merges arrays preserving numeric keys order
--FILE--
<?php
$a = array(1, 2);
$b = array(3, 4);
$c = array_merge($a, $b);
echo implode(',', $c) . "\n";
?>
--EXPECT--
1,2,3,4
--CLEAN--
<?php
unset($a, $b, $c);
