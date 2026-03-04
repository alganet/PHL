--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum skips null values
--FILE--
<?php
echo array_sum(array(1, null, 3));
?>
--EXPECT--
4
--CLEAN--
<?php

