--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: is_dir returns 1 for directories and 0 for non-directories
--FILE--
<?php
$here = __DIR__;
echo is_dir($here) ? "1\n" : "0\n";
// Non existent directory
$rand_name = __DIR__ . '/ph7_fake_dir_' . uniqid();
echo is_dir($rand_name) ? "1\n" : "0\n";
?>
--EXPECT--
1
0
--CLEAN--
<?php
unset($here, $rand_name);
