--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON_ERROR_DEPTH constant
--FILE--
<?php
echo "JSON_ERROR_DEPTH=" . JSON_ERROR_DEPTH . "\n";
?>
--EXPECT--
JSON_ERROR_DEPTH=1
--CLEAN--
<?php

