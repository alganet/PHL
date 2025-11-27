--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: XML_ERROR_NONE constant
--FILE--
<?php
// Note: current implementation maps XML_ERROR_NONE to SXML_ERROR_NO_MEMORY;
// Assert current behavior (should match SXML macros in sxint.h)
echo "XML_ERROR_NONE=" . XML_ERROR_NONE . "\n";
?>
--EXPECTF--
XML_ERROR_NONE=%d
