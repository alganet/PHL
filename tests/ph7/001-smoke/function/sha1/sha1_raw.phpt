--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sha1 raw binary output has length 20
--FILE--
<?php
$bin = sha1('abc', true);
echo strlen($bin) . "\n";
?>
--EXPECT--
20
--CLEAN--
<?php
unset($bin);
