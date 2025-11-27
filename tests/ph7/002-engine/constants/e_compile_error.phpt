--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: E_COMPILE_ERROR constant
--FILE--
<?php
echo "E_COMPILE_ERROR=" . E_COMPILE_ERROR . "\n";
?>
--EXPECTF--
E_COMPILE_ERROR=%d
