--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: mkdir and rmdir work under sys_get_temp_dir
--FILE--
<?php
$base = sys_get_temp_dir();
$dir = $base . '/ph7_test_dir_' . uniqid();
$ok = mkdir($dir);
echo "mkdir_ok=" . ($ok ? 'true' : 'false') . "\n";
echo "is_dir=" . (is_dir($dir) ? 'true' : 'false') . "\n";
$rm = rmdir($dir);
echo "rmdir_ok=" . ($rm ? 'true' : 'false') . "\n";
?>
--EXPECT--
mkdir_ok=true
is_dir=true
rmdir_ok=true
--CLEAN--
<?php
if (isset($dir) && is_dir($dir)) {
    rmdir($dir);
}
unset($base, $dir, $ok, $rm);
