--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON_UNESCAPED_LINE_TERMINATORS constant value
--FILE--
<?php
echo "JSON_UNESCAPED_LINE_TERMINATORS=" . JSON_UNESCAPED_LINE_TERMINATORS . "\n";
?>
--EXPECT--
JSON_UNESCAPED_LINE_TERMINATORS=2048
--CLEAN--
<?php
