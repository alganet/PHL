--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
md5 raw binary output has length 16
--FILE--
<?php
$bin = md5('abc', true);
echo strlen($bin) . "\n";
?>
--EXPECT--
16
--CLEAN--
<?php
unset($bin);
?>
