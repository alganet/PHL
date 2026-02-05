--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: EXTR_PREFIX_INVALID constant
--FILE--
<?php
echo "EXTR_PREFIX_INVALID=" . EXTR_PREFIX_INVALID . "\n";
?>
--EXPECTF--
EXTR_PREFIX_INVALID=%d
--CLEAN--
<?php

