--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String interpolation
--FILE--
<?php
$a = 5;
echo "$a";
echo "done";
?>
--EXPECT--
5done
--CLEAN--
<?php
unset($a);
