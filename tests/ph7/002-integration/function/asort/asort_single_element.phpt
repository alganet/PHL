--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
asort with single element array
--FILE--
<?php
$a = array('only' => 42);
echo asort($a) ? "true" : "false";
echo "\n";
foreach ($a as $k => $v) echo "$k: $v\n";
?>
--EXPECT--
true
only: 42
--CLEAN--
<?php
unset($a);
