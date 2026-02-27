--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_filter with a non-callable callback returns an empty array
--FILE--
<?php
$result = array_filter(array(1,2,3), "notfunc");
echo count($result) . "\n";
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_filter(): Argument #2 ($callback) must be a valid callback or null, function "notfunc" not found or invalid function name in %s
--CLEAN--
<?php
unset($result);
