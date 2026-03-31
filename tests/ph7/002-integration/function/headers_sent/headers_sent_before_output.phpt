--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
headers_sent() returns false before any output
--FILE--
<?php
$r = headers_sent();
echo $r ? "true" : "false";
echo "\n";
echo "output";
echo "\n";
$r = headers_sent();
echo $r ? "true" : "false";
?>
--EXPECT--
false
output
true
