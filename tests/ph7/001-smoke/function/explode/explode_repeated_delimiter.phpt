--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
explode with repeated delimiters includes empty entries
--FILE--
<?php
$x = explode('.', 'foo..bar');
echo "EXPLODE_REPEATED:COUNT:" . count($x) . "\n";
foreach ($x as $idx => $i) {
    echo "EXPLODE_REPEATED:ENTRY:" . $idx . ":START:" . $i . ":END\n";
}
?>
--EXPECT--
EXPLODE_REPEATED:COUNT:3
EXPLODE_REPEATED:ENTRY:0:START:foo:END
EXPLODE_REPEATED:ENTRY:1:START::END
EXPLODE_REPEATED:ENTRY:2:START:bar:END
--CLEAN--
<?php
unset($x);
