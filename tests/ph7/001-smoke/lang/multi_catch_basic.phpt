--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Multi-catch basic: catch (A | B $e) catches first and second type
--FILE--
<?php
class McBasicA extends Exception {}
class McBasicB extends Exception {}

try {
    throw new McBasicA("first");
} catch (McBasicA | McBasicB $e) {
    echo $e->getMessage() . "\n";
}

try {
    throw new McBasicB("second");
} catch (McBasicA | McBasicB $e) {
    echo $e->getMessage() . "\n";
}
?>
--EXPECT--
first
second
--CLEAN--
<?php
