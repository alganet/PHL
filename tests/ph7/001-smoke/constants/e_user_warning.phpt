--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: E_USER_WARNING constant
--FILE--
<?php
echo "E_USER_WARNING=" . E_USER_WARNING . "\n";
?>
--EXPECTF--
E_USER_WARNING=%d
--CLEAN--
<?php

