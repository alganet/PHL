--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Variable assignment and echo
--FILE--
<?php
$a = 42;
echo $a;
?>
--EXPECT--
42
--CLEAN--
<?php
unset($a);
?>