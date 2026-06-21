--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
return inside a catch runs the outer finally before returning (OFC ordering)
--FILE--
<?php
function rcfOuter() {
    try {
        try {
            throw new Exception("e");
        } catch (Exception $e) {
            return "C";
        }
    } finally {
        echo "OF";
    }
}
echo rcfOuter() . "\n";
?>
--EXPECT--
OFC
--CLEAN--
<?php
