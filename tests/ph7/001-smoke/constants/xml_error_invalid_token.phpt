--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: XML_ERROR_INVALID_TOKEN constant
--FILE--
<?php
echo "XML_ERROR_INVALID_TOKEN=" . XML_ERROR_INVALID_TOKEN . "\n";
?>
--EXPECTF--
XML_ERROR_INVALID_TOKEN=%d
--CLEAN--
<?php

