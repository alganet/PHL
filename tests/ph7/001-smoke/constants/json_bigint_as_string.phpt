--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON_BIGINT_AS_STRING constant value
--FILE--
<?php
echo "JSON_BIGINT_AS_STRING=" . JSON_BIGINT_AS_STRING . "\n";
?>
--EXPECT--
JSON_BIGINT_AS_STRING=2
--CLEAN--
<?php

