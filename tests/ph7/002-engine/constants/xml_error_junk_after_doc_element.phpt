--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: XML_ERROR_JUNK_AFTER_DOC_ELEMENT constant
--FILE--
<?php
echo "XML_ERROR_JUNK_AFTER_DOC_ELEMENT=" . XML_ERROR_JUNK_AFTER_DOC_ELEMENT . "\n";
?>
--EXPECTF--
XML_ERROR_JUNK_AFTER_DOC_ELEMENT=%d
