--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A catch-return survives a finally that calls a function with its own try/catch
--FILE--
<?php
function crf_helper() {
    // An inner throw+catch here used to clobber the VM-global pending-return
    // signal, dropping the caller's catch-return.
    try { throw new Exception("inner"); } catch (Exception $e) { /* swallow */ }
    return 1;
}
function crf_target() {
    try {
        throw new RuntimeException("boom");
    } catch (RuntimeException $e) {
        return "CAUGHT-RETURN";
    } finally {
        crf_helper();
    }
    return "FELL-THROUGH";
}
echo crf_target() . "\n";
?>
--EXPECT--
CAUGHT-RETURN
--CLEAN--
<?php
