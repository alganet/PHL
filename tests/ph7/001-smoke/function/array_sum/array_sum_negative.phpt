--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum with negative integers
--FILE--
<?php
echo array_sum(array(-1, -2, -3));
?>
--EXPECT--
-6
--CLEAN--
<?php

