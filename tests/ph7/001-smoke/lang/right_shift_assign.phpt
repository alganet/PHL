--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Right shift assignment operator >>=
--FILE--
<?php
$x = 16;
$x >>= 2;
if ($x == 4) {
    echo "pass\n";
} else {
    echo "fail\n";
}
?>
--EXPECT--
pass
--CLEAN--
<?php
unset($x);
