--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Multi-catch: three exception types
--FILE--
<?php
class McThreeA extends Exception {}
class McThreeB extends Exception {}
class McThreeC extends Exception {}

try {
    throw new McThreeC("three");
} catch (McThreeA | McThreeB | McThreeC $e) {
    echo $e->getMessage() . "\n";
}
?>
--EXPECT--
three
--CLEAN--
<?php
