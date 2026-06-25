--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A try-return survives a finally that calls a function with its own try/catch
--FILE--
<?php
function rtf_helper() {
    try { throw new Exception("inner"); } catch (Exception $e) { return 1; }
}
function rtf_target() {
    try {
        return "TRY-RETURN";
    } finally {
        rtf_helper();
    }
}
echo rtf_target() . "\n";
?>
--EXPECT--
TRY-RETURN
--CLEAN--
<?php
