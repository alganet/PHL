--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filectime() should return an integer timestamp for a file
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_ctime_');
file_put_contents($fname, 'A');
$t = filectime($fname);
echo "is_int=" . (is_int($t) ? 'true' : 'false') . PHP_EOL;
?>
--EXPECT--
is_int=true
--CLEAN--
<?php
@unlink($fname);
unset($fname, $t);
