--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON_ERROR_STATE_MISMATCH constant
--FILE--
<?php
echo "JSON_ERROR_STATE_MISMATCH=" . JSON_ERROR_STATE_MISMATCH . "\n";
?>
--EXPECT--
JSON_ERROR_STATE_MISMATCH=2
--CLEAN--
<?php

