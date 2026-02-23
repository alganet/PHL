--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert with empty string
--FILE--
<?php
echo base_convert("", 10, 16) . "\n";
?>
--EXPECT--

--CLEAN--
<?php

