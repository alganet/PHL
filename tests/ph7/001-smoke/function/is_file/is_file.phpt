--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: is_file returns 1 for files and 0 for non-files
--FILE--
<?php
$here = __FILE__;
echo is_file($here) ? "1\n" : "0\n";
// Non existent file
$rand_name = __DIR__ . '/ph7_fake_file_' . uniqid();
echo is_file($rand_name) ? "1\n" : "0\n";
?>
--EXPECT--
1
0
--CLEAN--
<?php
unset($here, $rand_name);
