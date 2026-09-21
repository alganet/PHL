--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fwrite $length edges: negative writes nothing, NULL means no limit, partial clamps
--FILE--
<?php
$fwl_fp = fopen('php://memory', 'r+');
var_dump(fwrite($fwl_fp, "abc", -1));
var_dump(fwrite($fwl_fp, "abc", 0));
var_dump(fwrite($fwl_fp, "abc", null));
var_dump(fwrite($fwl_fp, "de", 1));
var_dump(fwrite($fwl_fp, "fg", 99));
// a huge limit is "no limit", not a 32-bit-truncated negative
var_dump(fwrite($fwl_fp, "hi", PHP_INT_MAX));
var_dump(fwrite($fwl_fp, "jk", 2147483648));
rewind($fwl_fp);
var_dump(stream_get_contents($fwl_fp));
fclose($fwl_fp);
?>
--EXPECT--
int(0)
int(0)
int(3)
int(1)
int(2)
int(2)
int(2)
string(10) "abcdfghijk"
--CLEAN--
<?php
