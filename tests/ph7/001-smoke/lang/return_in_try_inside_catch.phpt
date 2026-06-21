--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
return in a try nested inside a catch runs the nested finally then returns
--FILE--
<?php
function rcfTryInCatch() {
    try {
        throw new Exception("e");
    } catch (Exception $e) {
        try {
            return "R";
        } finally {
            echo "nf";
        }
    }
}
echo rcfTryInCatch() . "\n";
?>
--EXPECT--
nfR
--CLEAN--
<?php
