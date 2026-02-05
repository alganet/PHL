--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array with empty string key
--FILE--
<?php
$arr = array("" => "value");
echo "Array with empty string key: " . $arr[""] . "\n";
?>
--EXPECT--
Array with empty string key: value
--CLEAN--
<?php
unset($arr);
