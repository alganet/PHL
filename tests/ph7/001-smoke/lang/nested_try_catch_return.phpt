--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
return from a catch nested inside another catch returns from the function
--FILE--
<?php
function rcfNested() {
    try {
        throw new Exception("outer");
    } catch (Exception $e) {
        try {
            throw new Exception("inner");
        } catch (Exception $e2) {
            return "inner";
        }
        return "outer-fallthrough";
    }
}
echo rcfNested() . "\n";
?>
--EXPECT--
inner
--CLEAN--
<?php
