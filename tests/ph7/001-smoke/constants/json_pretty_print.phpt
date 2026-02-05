--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON_PRETTY_PRINT constant value
--FILE--
<?php
echo "JSON_PRETTY_PRINT=" . JSON_PRETTY_PRINT . "\n";
?>
--EXPECT--
JSON_PRETTY_PRINT=128
--CLEAN--
<?php

