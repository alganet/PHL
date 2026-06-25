--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A finally can still override a catch-return after calling a helper
--FILE--
<?php
function crfo_helper() {
    try { throw new Exception("inner"); } catch (Exception $e) {}
    return 5;
}
function crfo_target() {
    try {
        throw new Exception("a");
    } catch (Exception $e) {
        return "X";
    } finally {
        crfo_helper();
        return "FINALLY";   // overrides the catch-return
    }
}
echo crfo_target() . "\n";
?>
--EXPECT--
FINALLY
--CLEAN--
<?php
