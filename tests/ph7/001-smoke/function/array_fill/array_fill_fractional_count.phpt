--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill should cast a fractional count to int (truncating)
--FILE--
<?php
echo "CNT1=" . count(array_fill(0, 2.5, 'x')) . "\n";
echo "CNT2=" . count(array_fill(0, -0.1, 'x')) . "\n";
?>
--EXPECTF--
Error [8192]: Implicit conversion from float 2.5 to int loses precision in %s on line %d
CNT1=2
Error [8192]: Implicit conversion from float -0.1 to int loses precision in %s on line %d
CNT2=0
--CLEAN--
<?php

