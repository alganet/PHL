--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_decode assoc variant
--FILE--
<?php
$str = '{"a":1, "b":[2,3]}';
$dec2 = json_decode($str, true);
echo is_array($dec2) ? "assoc\n" : "notassoc\n";
?>
--EXPECT--
assoc
--CLEAN--
<?php
unset($str, $dec2);
