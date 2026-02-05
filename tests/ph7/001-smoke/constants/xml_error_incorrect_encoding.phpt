--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: XML_ERROR_INCORRECT_ENCODING constant
--FILE--
<?php
echo "XML_ERROR_INCORRECT_ENCODING=" . XML_ERROR_INCORRECT_ENCODING . "\n";
?>
--EXPECTF--
XML_ERROR_INCORRECT_ENCODING=%d
--CLEAN--
<?php

