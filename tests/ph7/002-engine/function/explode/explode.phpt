--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
explode basic functionality
--FILE--
<?php
// Test basic explode
$array = explode(",", "a,b,c");
echo "EXPLODE_BASIC:COUNT:" . count($array) . "\n";
foreach ($array as $idx => $val) {
    echo "EXPLODE_BASIC:ENTRY:" . $idx . ":START:" . $val . ":END\n";
}
?>
--EXPECT--
EXPLODE_BASIC:COUNT:3
EXPLODE_BASIC:ENTRY:0:START:a:END
EXPLODE_BASIC:ENTRY:1:START:b:END
EXPLODE_BASIC:ENTRY:2:START:c:END
