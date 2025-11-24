--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_split splits a string into chunks of length 1
--FILE--
<?php
echo implode(':', str_split('abc', 1)) . "\n";
?>
--EXPECT--
a:b:c
