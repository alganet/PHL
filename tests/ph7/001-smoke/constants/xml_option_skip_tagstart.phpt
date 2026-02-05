--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: XML_OPTION_SKIP_TAGSTART constant
--FILE--
<?php
echo "XML_OPTION_SKIP_TAGSTART=" . XML_OPTION_SKIP_TAGSTART . "\n";
?>
--EXPECTF--
XML_OPTION_SKIP_TAGSTART=%d
--CLEAN--
<?php

