--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr_compare with invalid negative offset
--FILE--
<?php
$result = substr_compare("a", "b", -2);
if ($result === false) {
    echo "PASS";
} else {
    echo "FAIL";
}
?>
--EXPECTF--
%AFAIL%A
--CLEAN--
<?php
unset($result);
