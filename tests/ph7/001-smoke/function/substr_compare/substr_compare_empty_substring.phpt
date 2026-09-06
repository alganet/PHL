--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: substr_compare with empty substring returns FALSE
--FILE--
<?php
$result = substr_compare('abcdef', '', 0);
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
