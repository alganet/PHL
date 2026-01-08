--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: vprintf outputs formatted string using array arguments
--FILE--
<?php
$args = array("world", 42);
$len = vprintf("Hello %s, number %d\n", $args);
echo "Output length: " . $len . "\n";
?>
--EXPECT--
Hello world, number 42
Output length: 23
--CLEAN--
<?php
unset($args, $len);
?>