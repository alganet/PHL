--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: GLOB_ERR constant
--FILE--
<?php
echo "GLOB_ERR=" . GLOB_ERR . "\n";
?>
--EXPECTF--
GLOB_ERR=%d
--CLEAN--
<?php

