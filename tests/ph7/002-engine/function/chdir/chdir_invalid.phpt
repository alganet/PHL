--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chdir returns false for a non-existent directory and cwd remains unchanged
--FILE--
<?php
$cwd = getcwd();
$nonexistent = sys_get_temp_dir() . '/ph7_nonexistent_dir_8a1f9b';
$ok = @chdir($nonexistent);
echo ($ok ? '1' : '0') . PHP_EOL;
echo (getcwd() === $cwd ? '1' : '0') . PHP_EOL;
?>
--EXPECT--
0
1
--CLEAN--
<?php
chdir($cwd);
?>
