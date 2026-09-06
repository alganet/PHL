--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr returns false when negative offset points before string start
--FILE--
<?php
$result = substr("abc", -10);
if ($result === false) {
    echo "PASS: negative offset before string start returns false\n";
} else {
    echo "FAIL: expected false\n";
}
?>
--EXPECTF--
%AFAIL: expected false%A
--CLEAN--
<?php
unset($result);
