--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: __DIR__ constant returns current directory
--FILE--
<?php
echo "__DIR__=" . __DIR__ . "\n";
?>
--EXPECTF--
__DIR__=%s