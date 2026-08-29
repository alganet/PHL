--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strspn with empty mask returns 0

--FILE--
<?php
$result = strspn("hello", "");
if ($result === 0) {
    echo "PASS";
} else {
    echo "FAIL: got $result";
}
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($result);
