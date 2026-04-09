--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Multi-catch: first matching catch block wins
--FILE--
<?php
class McFirstA extends Exception {}
class McFirstB extends Exception {}

try {
    throw new McFirstA("from A");
} catch (McFirstA | McFirstB $e) {
    echo "multi: " . $e->getMessage() . "\n";
} catch (McFirstA $e) {
    echo "single: " . $e->getMessage() . "\n";
}
?>
--EXPECT--
multi: from A
--CLEAN--
<?php
