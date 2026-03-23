--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Return type of asin should be float
--FILE--
<?php
$result = asin(0.3);
echo is_float($result) ? "OK" : "FAIL";
?>
--EXPECT--
OK
--CLEAN--
<?php
unset($result);
