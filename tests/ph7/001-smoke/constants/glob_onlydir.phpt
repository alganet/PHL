--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: GLOB_ONLYDIR constant
--FILE--
<?php
echo "GLOB_ONLYDIR=" . GLOB_ONLYDIR . "\n";
?>
--EXPECT--
GLOB_ONLYDIR=1073741824
--CLEAN--
<?php

