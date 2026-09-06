--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr_compare with empty main string
--FILE--
<?php
$result = substr_compare("", "a", 0);
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
