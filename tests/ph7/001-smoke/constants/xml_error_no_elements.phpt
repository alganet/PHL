--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: XML_ERROR_NO_ELEMENTS constant
--FILE--
<?php
echo "XML_ERROR_NO_ELEMENTS=" . XML_ERROR_NO_ELEMENTS . "\n";
?>
--EXPECTF--
XML_ERROR_NO_ELEMENTS=%d
--CLEAN--
<?php

