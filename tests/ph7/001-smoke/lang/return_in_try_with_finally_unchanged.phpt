--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
return in a try body still runs finally then returns (regression guard)
--FILE--
<?php
function rcfTryFinNoOverride() {
    try {
        return "T";
    } finally {
        echo "f";
    }
}
function rcfTryFinOverride() {
    try {
        return "T";
    } finally {
        return "FIN";
    }
}
echo rcfTryFinNoOverride() . "\n";
echo rcfTryFinOverride() . "\n";
?>
--EXPECT--
fT
FIN
--CLEAN--
<?php
