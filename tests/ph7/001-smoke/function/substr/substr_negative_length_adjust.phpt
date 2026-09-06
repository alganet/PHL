--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: substr with negative length adjustment
--FILE--
<?php
$result = substr("a", 0, -2);
if ($result === "a") {
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
