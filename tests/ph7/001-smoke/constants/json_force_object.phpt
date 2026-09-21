--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON_FORCE_OBJECT constant value
--FILE--
<?php
echo "JSON_FORCE_OBJECT=" . JSON_FORCE_OBJECT . "\n";
?>
--EXPECT--
JSON_FORCE_OBJECT=16
--CLEAN--
<?php

