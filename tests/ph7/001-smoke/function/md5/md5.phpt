--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
md5 computes md5 hash for a string
--FILE--
<?php
echo md5('abc') . "\n";
?>
--EXPECT--
900150983cd24fb0d6963f7d28e17f72
--CLEAN--
<?php

