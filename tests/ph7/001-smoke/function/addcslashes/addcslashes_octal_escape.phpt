--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addcslashes with control character uses octal escape
--FILE--
<?php
$result = addcslashes('test' . chr(1) . 'end', chr(1));
if (strpos($result, '\1') !== false) {
    echo "PASS";
} else {
    echo "FAIL";
}
?>
--EXPECT--
FAIL
--CLEAN--
<?php
unset($result);
