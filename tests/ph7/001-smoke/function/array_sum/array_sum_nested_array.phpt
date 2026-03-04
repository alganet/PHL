--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum warns for nested array values
--FILE--
<?php
echo array_sum(array(1, array(2), 3));
?>
--EXPECTF--
Error [2]: array_sum(): Addition is not supported on type array in %s on line %s
4
--CLEAN--
<?php

