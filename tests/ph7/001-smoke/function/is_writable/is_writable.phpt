--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_writable() should return true for a writable file

--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_writable_');
file_put_contents($fname, 'A');
// Ensure file is writable
@chmod($fname, 0666);
echo "is_writable=" . (is_writable($fname) ? 'true' : 'false') . PHP_EOL;
@unlink($fname);
?>
--EXPECT--
is_writable=true
--CLEAN--
<?php
@unlink($fname);
unset($fname);
