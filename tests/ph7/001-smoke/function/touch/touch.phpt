--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
touch() should create file
--FILE--
<?php
$fname = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'ph7_touch_' . uniqid() . '.txt';
file_put_contents($fname, 'X');
$ok = touch($fname);
echo "created=" . (file_exists($fname) ? 'true' : 'false') . PHP_EOL;
$mtime1 = filemtime($fname);
?>
--EXPECT--
created=true
--CLEAN--
<?php
@unlink($fname);
unset($fname, $ok, $mtime1);
