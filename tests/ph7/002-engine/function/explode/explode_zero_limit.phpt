--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
explode() with zero limit returns entire string as first element
--FILE--
<?php
$arr = explode(",", "a,b,c,d", 0);
echo "count: " . count($arr) . "\n";
echo $arr[0] . "\n";
?>
--EXPECTF--
count: 1
a,b,c,d