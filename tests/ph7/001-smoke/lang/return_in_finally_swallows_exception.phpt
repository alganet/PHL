--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a return in finally swallows an exception re-thrown by the catch
--FILE--
<?php
function rcfSwallow() {
    try {
        throw new Exception("E1");
    } catch (Exception $e) {
        throw new RuntimeException("E2");
    } finally {
        return "FIN";
    }
}
echo rcfSwallow() . "\n";
?>
--EXPECT--
FIN
--CLEAN--
<?php
