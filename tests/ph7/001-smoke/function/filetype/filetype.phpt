--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: filetype returns 'file' for file and 'dir' for directory
--FILE--
<?php
// Test file type of current file and current directory
echo filetype(__FILE__) . "\n";
echo filetype(__DIR__) . "\n";
?>
--EXPECT--
file
dir
--CLEAN--
<?php

