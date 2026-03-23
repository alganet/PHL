--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert with numeric input converts correctly
--FILE--
<?php
echo base_convert(10, 10, 16) . "\n";
echo base_convert(255, 10, 16) . "\n";
?>
--EXPECT--
a
ff
--CLEAN--
<?php

