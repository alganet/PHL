--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
an exception thrown in finally discards a pending catch return
--FILE--
<?php
function rcfDiscardInner() {
    try {
        try {
            throw new Exception("E1");
        } catch (Exception $e) {
            return "C";
        } finally {
            throw new RuntimeException("E2");
        }
    } catch (RuntimeException $r) {
        return "got:" . $r->getMessage();
    }
}
echo rcfDiscardInner() . "\n";
?>
--EXPECT--
got:E2
--CLEAN--
<?php
