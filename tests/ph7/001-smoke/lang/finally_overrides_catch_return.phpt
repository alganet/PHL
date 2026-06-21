--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a return in finally overrides a return issued in the catch
--FILE--
<?php
function rcfOverride() {
    try {
        throw new Exception("e");
    } catch (Exception $e) {
        return "C";
    } finally {
        return "FIN";
    }
}
echo rcfOverride() . "\n";
?>
--EXPECT--
FIN
--CLEAN--
<?php
