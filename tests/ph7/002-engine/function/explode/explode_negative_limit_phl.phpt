--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
explode with negative limit PHL behavior
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$arr = explode(",", "a,b,c,d", -2);
echo "EXPLODE_NEGATIVE_LIMIT_PHL:COUNT:" . count($arr) . "\n";
foreach ($arr as $idx => $val) {
    echo "EXPLODE_NEGATIVE_LIMIT_PHL:ENTRY:" . $idx . ":START:" . $val . ":END\n";
}
?>
--EXPECT--
EXPLODE_NEGATIVE_LIMIT_PHL:COUNT:2
EXPLODE_NEGATIVE_LIMIT_PHL:ENTRY:0:START:a:END
EXPLODE_NEGATIVE_LIMIT_PHL:ENTRY:1:START:b,c,d:END
