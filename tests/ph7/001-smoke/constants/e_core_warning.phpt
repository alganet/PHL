--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: E_CORE_WARNING constant
--FILE--
<?php
echo "E_CORE_WARNING=" . E_CORE_WARNING . "\n";
?>
--EXPECTF--
E_CORE_WARNING=%d
--CLEAN--
<?php

