--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
bindec returns decimal value of binary string
--FILE--
<?php
echo bindec('10') . "\n"; // 2
?>
--EXPECT--
2
--CLEAN--
<?php

