--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr with offset beyond length
--FILE--
<?php
$result = substr('hello', 10);
if ($result === false) {
    echo "OFFSET_BEYOND_OK\n";
} else {
    echo "OFFSET_BEYOND_FAIL: '" . $result . "'\n";
}
?>
--EXPECTF--
%AOFFSET_BEYOND_FAIL: ''%A
--CLEAN--
<?php
unset($result);
