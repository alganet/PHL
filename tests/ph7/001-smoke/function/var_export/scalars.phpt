--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
var_export() scalars: null/bool/int/float/string (evaluable)
--FILE--
<?php

foreach ([null,true,false,0,1,-7,PHP_INT_MAX] as $v){ var_export($v); echo "\n"; }
foreach ([0.0,-0.0,0.1,1.5,1.0,100.0,1e20,1/3,1e-7,INF,-INF,NAN] as $v){ var_export($v); echo "\n"; }
foreach (["", "x", "a'b\\c", "two\nlines", "nul\0byte"] as $v){ var_export($v); echo "\n"; }
?>
--EXPECT--
NULL
true
false
0
1
-7
9223372036854775807
0.0
-0.0
0.1
1.5
1.0
100.0
1.0E+20
0.3333333333333333
1.0E-7
INF
-INF
NAN
''
'x'
'a\'b\\c'
'two
lines'
'nul' . "\0" . 'byte'
