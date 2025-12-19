--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array access with large integer key
--FILE--
<?php
$a = array();
$a[2147483647] = "large";
if ($a[2147483647] === "large") {
    echo "large_key_ok\n";
} else {
    echo "large_key_fail\n";
}
?>
--EXPECT--
large_key_ok