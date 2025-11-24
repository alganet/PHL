--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: E_WARNING constant value
--FILE--
<?php
echo "E_WARNING=" . E_WARNING . "\n";
?>
--EXPECT--
E_WARNING=2
