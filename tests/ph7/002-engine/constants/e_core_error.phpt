--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: E_CORE_ERROR constant
--FILE--
<?php
echo "E_CORE_ERROR=" . E_CORE_ERROR . "\n";
?>
--EXPECTF--
E_CORE_ERROR=%d
