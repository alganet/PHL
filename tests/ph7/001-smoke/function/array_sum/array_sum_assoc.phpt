--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum with associative array sums values regardless of keys
--FILE--
<?php
echo array_sum(array('a' => 10, 'b' => 20, 'c' => 30));
?>
--EXPECT--
60
--CLEAN--
<?php

