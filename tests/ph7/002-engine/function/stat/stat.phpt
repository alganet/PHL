--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
stat() should return an array for a regular file with correct size
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_stat_');
file_put_contents($fname, 'Hello');
$st = stat($fname);
echo "stat_size=" . $st['size'] . "\n";
echo "filesize=" . filesize($fname) . "\n";
?>
--EXPECT--
stat_size=5
filesize=5
--CLEAN--
<?php
@unlink($fname);
?>
