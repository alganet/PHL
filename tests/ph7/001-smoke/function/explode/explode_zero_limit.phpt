--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
explode with zero limit returns entire string as first element
--FILE--
<?php
$arr = explode(",", "a,b,c,d", 0);
echo "EXPLODE_ZERO_LIMIT:COUNT:" . count($arr) . "\n";
foreach ($arr as $idx => $val) {
    echo "EXPLODE_ZERO_LIMIT:ENTRY:" . $idx . ":START:" . $val . ":END\n";
}
?>
--EXPECT--
EXPLODE_ZERO_LIMIT:COUNT:1
EXPLODE_ZERO_LIMIT:ENTRY:0:START:a,b,c,d:END
--CLEAN--
<?php
unset($arr);
