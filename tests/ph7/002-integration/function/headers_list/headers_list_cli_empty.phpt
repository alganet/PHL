--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
headers_list() returns empty array in CLI mode
--FILE--
<?php
header("Content-Type: text/plain");
header("X-Custom: value");
$list = headers_list();
echo is_array($list) ? "array" : "not array";
echo "\n";
echo count($list);
?>
--EXPECT--
array
0
