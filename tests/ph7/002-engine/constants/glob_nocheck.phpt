--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: GLOB_NOCHECK constant
--FILE--
<?php
echo "GLOB_NOCHECK=" . GLOB_NOCHECK . "\n";
?>
--EXPECTF--
GLOB_NOCHECK=%d
