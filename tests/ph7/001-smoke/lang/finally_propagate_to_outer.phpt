--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
try/finally without catch propagates exception to outer try/catch
--FILE--
<?php
try {
    try {
        throw new Exception("inner");
    } finally {
        echo "inner finally\n";
    }
} catch (Exception $e) {
    echo "outer catch: " . $e->getMessage() . "\n";
} finally {
    echo "outer finally\n";
}
?>
--EXPECT--
inner finally
outer catch: inner
outer finally
--CLEAN--
<?php
