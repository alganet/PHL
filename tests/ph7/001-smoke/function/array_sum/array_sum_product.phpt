--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum and array_product simple cases
--FILE--
<?php
echo array_sum(array(1,2,3)) . PHP_EOL; // 6
echo array_product(array(2,3,4)) . PHP_EOL; // 24
?>
--EXPECT--
6
24
--CLEAN--
<?php

