--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: sys_get_temp_dir returns a non-empty string
--FILE--
<?php
$tmpdir = sys_get_temp_dir();
echo "sys_get_temp_dir_type=" . gettype($tmpdir) . "\n";
echo "sys_get_temp_dir_nonempty=" . ($tmpdir !== '' ? 'true' : 'false') . "\n";
?>
--EXPECT--
sys_get_temp_dir_type=string
sys_get_temp_dir_nonempty=true
--CLEAN--
<?php
unset($tmpdir);
