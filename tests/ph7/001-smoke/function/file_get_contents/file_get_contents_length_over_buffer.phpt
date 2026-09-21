--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
file_get_contents honours a $length larger than the internal read buffer (no over-read)
--FILE--
<?php
$fn = tempnam(sys_get_temp_dir(), 'ph7_fgcbig');
file_put_contents($fn, str_repeat('x', 10000));
// $length above the 8192-byte chunk buffer must not overshoot.
echo strlen(file_get_contents($fn, false, null, 0, 8192)), "\n";
echo strlen(file_get_contents($fn, false, null, 0, 8193)), "\n";
echo strlen(file_get_contents($fn, false, null, 500, 9000)), "\n";
echo strlen(file_get_contents($fn, false, null, 0, 9999)), "\n";
// A $length past EOF still stops at the file size.
echo strlen(file_get_contents($fn, false, null, 0, 20000)), "\n";
?>
--EXPECT--
8192
8193
9000
9999
10000
--CLEAN--
<?php
if (isset($fn) && file_exists($fn)) unlink($fn);
unset($fn);
