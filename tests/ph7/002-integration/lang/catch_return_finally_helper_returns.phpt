--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A catch-return survives a finally whose helper uses its own catch-return
--FILE--
<?php
function crhr_helper() {
    // The helper materializes its OWN catch-return (into its own result),
    // which consumes the shared signal — the caller's return must still survive.
    try { throw new Exception("inner"); } catch (Exception $e) { return 99; }
}
function crhr_target() {
    try {
        throw new Exception("a");
    } catch (Exception $e) {
        return "OUTER";
    } finally {
        $x = crhr_helper();
        echo "helper=$x\n";
    }
}
echo crhr_target() . "\n";
?>
--EXPECT--
helper=99
OUTER
--CLEAN--
<?php
