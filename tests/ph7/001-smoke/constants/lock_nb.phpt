--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: LOCK_NB constant
--FILE--
<?php
echo "LOCK_NB=" . LOCK_NB . "\n";
?>
--EXPECT--
LOCK_NB=4
--CLEAN--
<?php

