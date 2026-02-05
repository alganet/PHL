--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: PATHINFO_BASENAME should be 2
--FILE--
<?php
echo PATHINFO_BASENAME . "\n";
?>
--EXPECT--
2
--CLEAN--
<?php

