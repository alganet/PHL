--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Keyed list destructuring: missing key yields null
--FILE--
<?php
set_error_handler(function ($n, $s) { return true; }); // swallow PHP's Undefined-array-key warning
["x" => $v] = ["y" => 1];
var_export($v);
echo "\n";
restore_error_handler();
?>
--EXPECT--
NULL
--CLEAN--
<?php
unset($v);
