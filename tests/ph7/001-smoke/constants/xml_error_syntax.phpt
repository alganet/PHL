--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: XML_ERROR_SYNTAX constant
--FILE--
<?php
echo "XML_ERROR_SYNTAX=" . XML_ERROR_SYNTAX . "\n";
?>
--EXPECTF--
XML_ERROR_SYNTAX=%d
--CLEAN--
<?php

