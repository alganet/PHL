--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: pathinfo returns components and supports option flags
--FILE--
<?php
$path = 'a/b/c.txt';
// DIRNAME
echo pathinfo($path, PATHINFO_DIRNAME) . "\n";
// BASENAME
echo pathinfo($path, PATHINFO_BASENAME) . "\n";
// EXTENSION
echo pathinfo($path, PATHINFO_EXTENSION) . "\n";
// FILENAME
echo pathinfo($path, PATHINFO_FILENAME) . "\n";
// Default returns array; print the extension element
$info = pathinfo($path);
echo $info['extension'] . "\n";
?>
--EXPECT--
a/b
c.txt
txt
c
txt
