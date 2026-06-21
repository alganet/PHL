--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
finally runs after a catch that returns, before the function returns
--FILE--
<?php
function rcfRunsFinally() {
    try {
        throw new Exception("e");
    } catch (Exception $e) {
        echo "c";
        return "C";
    } finally {
        echo "f";
    }
}
echo rcfRunsFinally() . "\n";
?>
--EXPECT--
cfC
--CLEAN--
<?php
