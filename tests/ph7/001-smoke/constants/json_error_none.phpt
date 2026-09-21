--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON_ERROR_NONE constant
--FILE--
<?php
echo "JSON_ERROR_NONE=" . JSON_ERROR_NONE . "\n";
?>
--EXPECT--
JSON_ERROR_NONE=0
--CLEAN--
<?php

