--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nested try/catch/finally
--FILE--
<?php
try {
    echo "outer try\n";
    try {
        echo "inner try\n";
        throw new Exception("inner");
    } catch (Exception $e) {
        echo "inner catch\n";
    } finally {
        echo "inner finally\n";
    }
} catch (Exception $e) {
    echo "outer catch\n";
} finally {
    echo "outer finally\n";
}
?>
--EXPECT--
outer try
inner try
inner catch
inner finally
outer finally
--CLEAN--
<?php
