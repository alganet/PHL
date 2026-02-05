--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: XML_ERROR_BAD_CHAR_REF constant
--FILE--
<?php
echo "XML_ERROR_BAD_CHAR_REF=" . XML_ERROR_BAD_CHAR_REF . "\n";
?>
--EXPECTF--
XML_ERROR_BAD_CHAR_REF=%d
--CLEAN--
<?php

