--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: static typed property rejects wrong type
--FILE--
<?php
class TpiRegistry { public static int $count = 0; }
try {
    TpiRegistry::$count = [1, 2];
} catch (TypeError $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
echo TpiRegistry::$count, "\n";
TpiRegistry::$count = 5;
echo TpiRegistry::$count, "\n";
?>
--EXPECT--
caught: Cannot assign array to property TpiRegistry::$count of type int
0
5
--CLEAN--
<?php
