--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum with numeric strings parses them as integers
--FILE--
<?php
echo array_sum(array("3", "4"));
?>
--EXPECT--
7
--CLEAN--
<?php

