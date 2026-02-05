--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: GLOB_NOSORT constant
--FILE--
<?php
echo "GLOB_NOSORT=" . GLOB_NOSORT . "\n";
?>
--EXPECTF--
GLOB_NOSORT=%d
--CLEAN--
<?php

