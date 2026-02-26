--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill: negative start index is allowed and used as key
--FILE--
<?php
$a = array_fill(-2, 2, 'z');
echo count($a) . "\n";
echo array_keys($a)[0] . "\n";
?>
--EXPECT--
2
-2
--CLEAN--
<?php
unset($a);
