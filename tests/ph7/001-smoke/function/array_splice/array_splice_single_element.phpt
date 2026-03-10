--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_splice on single element array removes the element
--FILE--
<?php
$a = array(42);
$r = array_splice($a, 0);
echo implode(',', $r) . "\n";
echo count($a);
?>
--EXPECT--
42
0
--CLEAN--
<?php
unset($a, $r);
