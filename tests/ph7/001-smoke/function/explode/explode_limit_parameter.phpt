--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
explode limit parameter functionality
--FILE--
<?php
// Test with positive limit
$arr1 = explode(",", "a,b,c,d", 3);
echo "EXPLODE_LIMIT_POS:COUNT:" . count($arr1) . "\n";
foreach ($arr1 as $idx => $val) {
    echo "EXPLODE_LIMIT_POS:ENTRY:" . $idx . ":START:" . $val . ":END\n";
}

// Test with limit equal to number of elements
$arr2 = explode(",", "a,b,c", 3);
echo "EXPLODE_LIMIT_EQUAL:COUNT:" . count($arr2) . "\n";
foreach ($arr2 as $idx => $val) {
    echo "EXPLODE_LIMIT_EQUAL:ENTRY:" . $idx . ":START:" . $val . ":END\n";
}
?>
--EXPECT--
EXPLODE_LIMIT_POS:COUNT:3
EXPLODE_LIMIT_POS:ENTRY:0:START:a:END
EXPLODE_LIMIT_POS:ENTRY:1:START:b:END
EXPLODE_LIMIT_POS:ENTRY:2:START:c,d:END
EXPLODE_LIMIT_EQUAL:COUNT:3
EXPLODE_LIMIT_EQUAL:ENTRY:0:START:a:END
EXPLODE_LIMIT_EQUAL:ENTRY:1:START:b:END
EXPLODE_LIMIT_EQUAL:ENTRY:2:START:c:END
--CLEAN--
<?php
unset($arr1, $arr2);
