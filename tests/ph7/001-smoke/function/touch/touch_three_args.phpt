--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
touch() with three arguments
--SKIPIF--
<?php
if (PHP_OS == 'WINNT') {
    echo "skip";
}
?>
--FILE--
<?php
$fname = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'ph7_touch3_' . uniqid() . '.txt';
file_put_contents($fname, 'X');
$time = time();
$ok = touch($fname, $time, $time);
echo "touched=" . ($ok ? 'true' : 'false') . PHP_EOL;
?>
--EXPECT--
touched=true
--CLEAN--
<?php
@unlink($fname);
unset($fname, $time, $ok);
