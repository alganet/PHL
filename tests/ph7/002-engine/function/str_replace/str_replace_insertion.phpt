--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_replace: replacement longer than search and deletion behavior
--FILE--
<?php
// Replacement longer than search: should insert full replace string
echo str_replace('a', 'ABCDEFGHI', 'xa') . PHP_EOL;
// Zero-length replacement deletes the target substring
echo str_replace('b', '', 'xb') . PHP_EOL;
?>
--EXPECT--
xABCDEFGHI
x
