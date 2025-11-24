--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: E_ALL constant value
--FILE--
<?php
echo "E_ALL=" . E_ALL . "\n";
?>
--EXPECTF--
E_ALL=%d
