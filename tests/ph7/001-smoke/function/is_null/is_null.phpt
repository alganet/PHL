--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_null function
--FILE--
<?php
echo is_null(null) ? "TRUE" : "FALSE";
echo is_null(0) ? "TRUE" : "FALSE";
echo is_null("") ? "TRUE" : "FALSE";
?>
--EXPECT--
TRUEFALSEFALSE
--CLEAN--
<?php

