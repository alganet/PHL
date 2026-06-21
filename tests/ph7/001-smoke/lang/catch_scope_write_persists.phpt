--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a variable assigned inside a catch block is visible after the try/catch
--FILE--
<?php
function inFunc() {
    try {
        throw new Exception("e");
    } catch (Exception $e) {
        $w = "W";
    }
    return isset($w) ? $w : "UNSET";
}
echo inFunc() . "\n";

// global scope
try {
    throw new Exception("e");
} catch (Exception $e) {
    $y = "SET";
}
echo (isset($y) ? $y : "UNSET") . "\n";
?>
--EXPECT--
W
SET
--CLEAN--
<?php
