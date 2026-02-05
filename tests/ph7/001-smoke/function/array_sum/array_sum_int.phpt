--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum with integers
--FILE--
<?php
echo array_sum(array(1, 2, 3, 4, 5)) . "\n";
?>
--EXPECT--
15
--CLEAN--
<?php

