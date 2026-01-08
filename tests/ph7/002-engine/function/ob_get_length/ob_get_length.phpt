--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ob_get_length returns buffer length
--FILE--
<?php
ob_start();
echo "Hello";
$len = ob_get_length();
ob_end_clean();
if ($len === 5) {
    echo "ok\n";
} else {
    echo "fail: expected 5, got $len\n";
}
?>
--EXPECT--
ok