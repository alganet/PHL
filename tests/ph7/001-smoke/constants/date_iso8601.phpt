--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: DATE_ISO8601 format string is correct
--FILE--
<?php
echo "DATE_ISO8601=" . DATE_ISO8601 . "\n";
?>
--EXPECT--
DATE_ISO8601=Y-m-d\TH:i:sO
--CLEAN--
<?php

