--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: INI_SCANNER_NORMAL constant
--FILE--
<?php
echo "INI_SCANNER_NORMAL=" . INI_SCANNER_NORMAL . "\n";
?>
--EXPECTF--
INI_SCANNER_NORMAL=%d
--CLEAN--
<?php

