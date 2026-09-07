--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert with null input
--FILE--
<?php
$result = base_convert(null, 10, 10);
if ($result === "0") {
    echo "PASS";
} else {
    echo "FAIL";
}
?>
--EXPECTF--
%Abase_convert(): Passing null to parameter #1 ($num) of type string is deprecated%APASS%A
--CLEAN--
<?php
unset($result);
