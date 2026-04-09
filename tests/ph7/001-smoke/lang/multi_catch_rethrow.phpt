--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Multi-catch: rethrow from multi-catch with finally
--FILE--
<?php
class McRethrowA extends Exception {}
class McRethrowB extends Exception {}

try {
    try {
        throw new McRethrowB("rethrown");
    } catch (McRethrowA | McRethrowB $e) {
        echo "inner: " . $e->getMessage() . "\n";
        throw $e;
    } finally {
        echo "finally\n";
    }
} catch (McRethrowB $e) {
    echo "outer: " . $e->getMessage() . "\n";
}
?>
--EXPECT--
inner: rethrown
finally
outer: rethrown
--CLEAN--
<?php
