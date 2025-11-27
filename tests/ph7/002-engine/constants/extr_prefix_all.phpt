--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: EXTR_PREFIX_ALL constant
--FILE--
<?php
echo "EXTR_PREFIX_ALL=" . EXTR_PREFIX_ALL . "\n";
?>
--EXPECTF--
EXTR_PREFIX_ALL=%d
