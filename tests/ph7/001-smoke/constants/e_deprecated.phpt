--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: E_DEPRECATED constant
--FILE--
<?php
echo "E_DEPRECATED=" . E_DEPRECATED . "\n";
?>
--EXPECTF--
E_DEPRECATED=%d
--CLEAN--
<?php

