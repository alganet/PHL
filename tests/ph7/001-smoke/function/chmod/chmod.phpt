--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chmod() should change file permissions

--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_chmod_');
file_put_contents($fname, 'A');
$ok = chmod($fname, 0600);
echo "chmod_ok=" . ($ok ? 'true' : 'false') . PHP_EOL;
// Check if writable
echo "is_writable_now=" . (is_writable($fname) ? 'true' : 'false') . PHP_EOL;
?>
--EXPECT--
chmod_ok=true
is_writable_now=true
--CLEAN--
<?php
@chmod($fname, 0666);
@unlink($fname);
unset($fname, $ok);
