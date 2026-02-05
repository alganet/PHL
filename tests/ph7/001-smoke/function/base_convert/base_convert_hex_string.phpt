--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert with hex string input
--FILE--
<?php
echo base_convert("ff",16,10) . "\n";
?>
--EXPECT--
255
--CLEAN--
<?php

