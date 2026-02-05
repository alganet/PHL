--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON_UNESCAPED_UNICODE constant value
--FILE--
<?php
echo "JSON_UNESCAPED_UNICODE=" . JSON_UNESCAPED_UNICODE . "\n";
?>
--EXPECTF--
JSON_UNESCAPED_UNICODE=%d
--CLEAN--
<?php

