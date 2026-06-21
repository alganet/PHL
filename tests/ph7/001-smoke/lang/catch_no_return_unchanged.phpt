--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a catch without return still falls through to the code after the try/catch
--FILE--
<?php
function rcfNoReturn() {
    try {
        throw new Exception("e");
    } catch (Exception $e) {
        echo "c";
    }
    echo "after";
    return "END";
}
echo rcfNoReturn() . "\n";
?>
--EXPECT--
cafterEND
--CLEAN--
<?php
