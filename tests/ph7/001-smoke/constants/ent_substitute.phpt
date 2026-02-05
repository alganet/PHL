--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ENT_SUBSTITUTE constant
--FILE--
<?php
echo "ENT_SUBSTITUTE=" . ENT_SUBSTITUTE . "\n";
?>
--EXPECTF--
ENT_SUBSTITUTE=%d
--CLEAN--
<?php

