--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Trailing comma in isset() and unset() calls
--FILE--
<?php
$a = 1;
echo isset($a,) ? "yes" : "no";
echo "\n";
unset($a,);
echo isset($a) ? "yes" : "no";
echo "\n";
?>
--EXPECT--
yes
no
--CLEAN--
<?php
