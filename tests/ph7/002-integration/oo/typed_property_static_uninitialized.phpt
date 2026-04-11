--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: reading uninitialized static typed property throws Error
--FILE--
<?php
class TpsuRegistry { public static int $count; }
try {
    echo TpsuRegistry::$count, "\n";
} catch (Error $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
TpsuRegistry::$count = 7;
echo TpsuRegistry::$count, "\n";
?>
--EXPECT--
caught: Typed static property TpsuRegistry::$count must not be accessed before initialization
7
--CLEAN--
<?php
