--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
return inside a finally block (normal try completion) returns from the function
--FILE--
<?php
function rcfFinally() {
    try {
        echo "t";
    } finally {
        return "FIN";
    }
}
echo rcfFinally() . "\n";
?>
--EXPECT--
tFIN
--CLEAN--
<?php
