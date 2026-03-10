--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count returns number of elements in an associative array
--FILE--
<?php
echo count(array('a' => 1, 'b' => 2, 'c' => 3, 'd' => 4));
?>
--EXPECT--
4
--CLEAN--
<?php

