--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: E_PARSE constant
--FILE--
<?php
echo "E_PARSE=" . E_PARSE . "\n";
?>
--EXPECTF--
E_PARSE=%d
--CLEAN--
<?php

