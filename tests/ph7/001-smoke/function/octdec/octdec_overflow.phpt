--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
octdec promotes to float once the value exceeds PHP_INT_MAX
--FILE--
<?php
$v = octdec("1777777777777777777777");  // 2^64 - 1
echo is_float($v) ? "float " : "int ";
echo $v > PHP_INT_MAX ? "big " : "notbig ";
echo ($v == 1.8446744073709552e19) ? "eq\n" : "ne\n";
echo octdec("777777777777777777777"), "\n";  // 2^63 - 1: stays int
echo octdec("777"), "\n";                     // small control
?>
--EXPECT--
float big eq
9223372036854775807
511
--CLEAN--
<?php
