--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Multiple string interpolations
--FILE--
<?php
$a = 1;
$b = 2;
echo "a=$a b=$b";
echo "done";
?>
--EXPECT--
a=1 b=2done