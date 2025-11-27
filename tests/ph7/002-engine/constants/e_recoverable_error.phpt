--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: E_RECOVERABLE_ERROR constant
--FILE--
<?php
echo "E_RECOVERABLE_ERROR=" . E_RECOVERABLE_ERROR . "\n";
?>
--EXPECTF--
E_RECOVERABLE_ERROR=%d
