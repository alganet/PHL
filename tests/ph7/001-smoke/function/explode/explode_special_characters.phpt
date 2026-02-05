--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
explode with special character delimiters
--FILE--
<?php
// Test with space delimiter
$arr1 = explode(" ", "hello world test");
echo "EXPLODE_SPACE:COUNT:" . count($arr1) . "\n";
foreach ($arr1 as $idx => $val) {
    echo "EXPLODE_SPACE:ENTRY:" . $idx . ":START:" . $val . ":END\n";
}

// Test with tab delimiter
$arr2 = explode("\t", "a\tb\tc");
echo "EXPLODE_TAB:COUNT:" . count($arr2) . "\n";
foreach ($arr2 as $idx => $val) {
    echo "EXPLODE_TAB:ENTRY:" . $idx . ":START:" . $val . ":END\n";
}

// Test with newline delimiter
$arr3 = explode("\n", "line1\nline2\nline3");
echo "EXPLODE_NEWLINE:COUNT:" . count($arr3) . "\n";
foreach ($arr3 as $idx => $val) {
    echo "EXPLODE_NEWLINE:ENTRY:" . $idx . ":START:" . $val . ":END\n";
}
?>
--EXPECT--
EXPLODE_SPACE:COUNT:3
EXPLODE_SPACE:ENTRY:0:START:hello:END
EXPLODE_SPACE:ENTRY:1:START:world:END
EXPLODE_SPACE:ENTRY:2:START:test:END
EXPLODE_TAB:COUNT:3
EXPLODE_TAB:ENTRY:0:START:a:END
EXPLODE_TAB:ENTRY:1:START:b:END
EXPLODE_TAB:ENTRY:2:START:c:END
EXPLODE_NEWLINE:COUNT:3
EXPLODE_NEWLINE:ENTRY:0:START:line1:END
EXPLODE_NEWLINE:ENTRY:1:START:line2:END
EXPLODE_NEWLINE:ENTRY:2:START:line3:END
--CLEAN--
<?php
unset($arr1, $arr2, $arr3);
