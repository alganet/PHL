--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: DATE_ATOM format string is correct
--FILE--
<?php
echo "DATE_ATOM=" . DATE_ATOM . "\n";
?>
--EXPECT--
DATE_ATOM=Y-m-d\TH:i:sP
