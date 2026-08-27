--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strftime() emits php 8.1's E_DEPRECATED with the exact message
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "[$no] $str\n"; return true; });
$out = strftime("%Y");
echo is_string($out) && strlen($out) === 4 ? "shape-ok" : "shape-bad", "\n";
?>
--EXPECT--
[8192] Function strftime() is deprecated since 8.1, use IntlDateFormatter::format() instead
shape-ok
