--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill: basic count works
--FILE--
<?php
$a = array_fill(0, 3, 'x');
echo count($a) . "\n";
?>
--EXPECT--
3
--CLEAN--
<?php
unset($a);
