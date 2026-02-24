--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
NaN comparison semantics and relational behavior
--FILE--
<?php
echo NAN == NAN ? "T\n" : "F\n";
echo NAN != NAN ? "T\n" : "F\n";
echo NAN === NAN ? "T\n" : "F\n";
echo NAN !== NAN ? "T\n" : "F\n";

echo NAN < 1 ? "T\n" : "F\n";
echo NAN <= 1 ? "T\n" : "F\n";
echo NAN > 1 ? "T\n" : "F\n";
echo NAN >= 1 ? "T\n" : "F\n";

echo 1 < NAN ? "T\n" : "F\n";
echo 1 <= NAN ? "T\n" : "F\n";
echo 1 > NAN ? "T\n" : "F\n";
echo 1 >= NAN ? "T\n" : "F\n";
?>
--EXPECT--
F
T
F
T
F
F
F
F
F
F
F
F
--CLEAN--
<?php

