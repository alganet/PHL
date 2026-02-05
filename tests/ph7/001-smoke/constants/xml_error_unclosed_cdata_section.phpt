--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: XML_ERROR_UNCLOSED_CDATA_SECTION constant
--FILE--
<?php
echo "XML_ERROR_UNCLOSED_CDATA_SECTION=" . XML_ERROR_UNCLOSED_CDATA_SECTION . "\n";
?>
--EXPECTF--
XML_ERROR_UNCLOSED_CDATA_SECTION=%d
--CLEAN--
<?php

