--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: XML_ERROR_ASYNC_ENTITY constant
--FILE--
<?php
echo "XML_ERROR_ASYNC_ENTITY=" . XML_ERROR_ASYNC_ENTITY . "\n";
?>
--EXPECTF--
XML_ERROR_ASYNC_ENTITY=%d
--CLEAN--
<?php

