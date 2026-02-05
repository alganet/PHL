--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: HTML_ENTITIES constant
--FILE--
<?php
echo "HTML_ENTITIES=" . HTML_ENTITIES . "\n";
?>
--EXPECTF--
HTML_ENTITIES=%d
--CLEAN--
<?php

