--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: HTML_SPECIALCHARS constant
--FILE--
<?php
echo "HTML_SPECIALCHARS=" . HTML_SPECIALCHARS . "\n";
?>
--EXPECTF--
HTML_SPECIALCHARS=%d
--CLEAN--
<?php

