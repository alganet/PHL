--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Multi-catch: no match falls through to next catch block
--FILE--
<?php
class McFallA extends Exception {}
class McFallB extends Exception {}
class McFallC extends Exception {}

try {
    throw new McFallC("from C");
} catch (McFallA | McFallB $e) {
    echo "AB: " . $e->getMessage() . "\n";
} catch (McFallC $e) {
    echo "C: " . $e->getMessage() . "\n";
}
?>
--EXPECT--
C: from C
--CLEAN--
<?php
