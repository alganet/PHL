--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
try/catch/finally: basic finally execution
--FILE--
<?php
function test1() {
    try {
        echo "try\n";
    } catch (Exception $e) {
        echo "catch\n";
    } finally {
        echo "finally\n";
    }
}

function test2() {
    try {
        echo "try\n";
        throw new Exception("error");
    } catch (Exception $e) {
        echo "catch\n";
    } finally {
        echo "finally\n";
    }
}

echo "=== no exception ===\n";
test1();
echo "=== with exception ===\n";
test2();
?>
--EXPECT--
=== no exception ===
try
finally
=== with exception ===
try
catch
finally
--CLEAN--
<?php
