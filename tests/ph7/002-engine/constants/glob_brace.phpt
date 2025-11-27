--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: GLOB_BRACE constant
--FILE--
<?php
echo "GLOB_BRACE=" . GLOB_BRACE . "\n";
?>
--EXPECTF--
GLOB_BRACE=%d
