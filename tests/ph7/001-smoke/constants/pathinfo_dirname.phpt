--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: PATHINFO_DIRNAME constant value
--FILE--
<?php
echo "PATHINFO_DIRNAME=" . PATHINFO_DIRNAME . "\n";
?>
--EXPECT--
PATHINFO_DIRNAME=1
--CLEAN--
<?php

