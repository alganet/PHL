--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
asort with empty array returns true
--FILE--
<?php
$a = array();
echo asort($a) ? "true" : "false";
echo "\n";
echo count($a);
?>
--EXPECT--
true
0
--CLEAN--
<?php
unset($a);
