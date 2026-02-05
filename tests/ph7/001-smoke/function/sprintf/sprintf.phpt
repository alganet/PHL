--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sprintf formats a string similarly to standard sprintf
--FILE--
<?php
$ret = sprintf('%.2f', 3.14159);
echo $ret . "\n";
?>
--EXPECT--
3.14
--CLEAN--
<?php
unset($ret);
