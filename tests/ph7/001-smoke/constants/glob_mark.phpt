--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: GLOB_MARK constant
--FILE--
<?php
echo "GLOB_MARK=" . GLOB_MARK . "\n";
?>
--EXPECTF--
GLOB_MARK=%d
--CLEAN--
<?php

