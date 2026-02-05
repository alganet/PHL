--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: EXTR_SKIP constant
--FILE--
<?php
echo "EXTR_SKIP=" . EXTR_SKIP . "\n";
?>
--EXPECTF--
EXTR_SKIP=%d
--CLEAN--
<?php

