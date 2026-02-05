--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: XML_ERROR_EXTERNAL_ENTITY_HANDLING constant
--FILE--
<?php
echo "XML_ERROR_EXTERNAL_ENTITY_HANDLING=" . XML_ERROR_EXTERNAL_ENTITY_HANDLING . "\n";
?>
--EXPECTF--
XML_ERROR_EXTERNAL_ENTITY_HANDLING=%d
--CLEAN--
<?php

