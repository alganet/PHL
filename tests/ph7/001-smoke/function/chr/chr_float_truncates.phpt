--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chr(3.9) truncates to chr(3) with deprecation
--FILE--
<?php
echo ord(chr(3.9)) . "\n";
?>
--EXPECTF--
Error [%d]: Implicit conversion from float %f to int loses precision in %s on line %d
3
--CLEAN--
<?php

