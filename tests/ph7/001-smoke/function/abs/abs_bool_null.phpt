--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: abs on boolean values coerces to integer
--FILE--
<?php
echo abs(true) . "," . abs(false);
?>
--EXPECT--
1,0
--CLEAN--
<?php

