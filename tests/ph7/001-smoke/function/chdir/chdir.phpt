--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chdir should change the current working dir and getcwd reflects it
?>
--FILE--
<?php
$cwd = getcwd();
$dir = sys_get_temp_dir();
$ok = chdir($dir);
echo "chdir_ok=" . ($ok ? 'true' : 'false') . PHP_EOL;
echo "cwd_contains=" . (strpos(getcwd(), basename($dir)) !== false ? 'true' : 'false') . PHP_EOL;
?>
--EXPECT--
chdir_ok=true
cwd_contains=true
--CLEAN--
<?php
chdir($cwd);
unset($cwd, $dir, $ok);
