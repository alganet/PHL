--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
finally runs even when catch block throws
--FILE--
<?php
try {
    try {
        throw new Exception("first");
    } catch (Exception $e) {
        echo "catch: " . $e->getMessage() . "\n";
        throw new Exception("second");
    } finally {
        echo "inner finally\n";
    }
} catch (Exception $e) {
    echo "outer catch: " . $e->getMessage() . "\n";
}
?>
--EXPECT--
catch: first
inner finally
outer catch: second
--CLEAN--
<?php
