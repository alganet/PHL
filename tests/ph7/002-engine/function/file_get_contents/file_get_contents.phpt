--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: file_get_contents reads a temp file
--FILE--
<?php
$filename = tempnam(sys_get_temp_dir(), 'ph7_test');
file_put_contents($filename, "hello\n");
$contents = file_get_contents($filename);
echo "contents=" . rtrim($contents, "\n") . "\n";
?>
--EXPECT--
contents=hello
--CLEAN--
<?php
unlink($filename);
unset($filename);
?>
