--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sprintf supports format with width and precision
--FILE--
<?php
$ret = sprintf('%10.5s', 'Hello World');
echo "padded=" . $ret . "\n";
?>
--EXPECT--
padded=     Hello
--CLEAN--
<?php
unset($ret);
