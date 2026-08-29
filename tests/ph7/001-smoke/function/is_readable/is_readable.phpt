--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_readable() should return true for a readable file

--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_readable_');
file_put_contents($fname, 'A');
// Ensure file is readable
@chmod($fname, 0777);
echo "is_readable=" . (is_readable($fname) ? 'true' : 'false') . PHP_EOL;
?>
--EXPECT--
is_readable=true
--CLEAN--
<?php
@unlink($fname);
unset($fname);
