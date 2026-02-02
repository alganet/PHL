--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
explode with empty string input
--FILE--
<?php
$arr = explode(",", "");
echo "EXPLODE_EMPTY_STRING:COUNT:" . count($arr) . "\n";
foreach ($arr as $idx => $val) {
    echo "EXPLODE_EMPTY_STRING:ENTRY:" . $idx . ":START:" . $val . ":END\n";
}
?>
--EXPECT--
EXPLODE_EMPTY_STRING:COUNT:1
EXPLODE_EMPTY_STRING:ENTRY:0:START::END