--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill: negative count throws ValueError
--FILE--
<?php
// Both PHP and PHL should raise the same ValueError message
array_fill(0, -1, 'x');
?>
--EXPECTF--
PHP Fatal error:  Uncaught ValueError: array_fill(): Argument #2 ($count) must be greater than or equal to 0 in %s
--CLEAN--
<?php

