--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
NAN constant should be a float and behave like NaN
--FILE--
<?php
// constant must exist and be a float
echo gettype(NAN) . "\n";
// NaN should not equal itself
$result = NAN;
echo $result != $result ? "OK" : "FAIL" . $result;
?>
--EXPECT--
float
OK
--CLEAN--
<?php
?>
