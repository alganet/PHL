--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: sprintf with invalid argument types

--FILE--
<?php
// Test sprintf with wrong argument types for format specifiers
$result = sprintf("%d", "not_a_number");
if ($result === "0") {
    echo "PASS";
} else {
    echo "FAIL";
}
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($result);
