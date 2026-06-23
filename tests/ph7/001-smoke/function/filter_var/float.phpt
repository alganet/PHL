--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var FILTER_VALIDATE_FLOAT: decimal/scientific, thousand grouping, trim
--FILE--
<?php
if(!function_exists("fv_show")){function fv_show($v){if($v===true)echo "T";elseif($v===false)echo "F";elseif($v===null)echo "N";elseif(is_int($v))echo "i:$v";elseif(is_float($v))echo "f:".(0+$v);else echo "s:$v";echo "
";}}

foreach (["1.5",".5","5.","1e3","1.5e-3","abc","0x1A"," 1.5 ","1,000"] as $x) fv_show(filter_var($x, FILTER_VALIDATE_FLOAT));
foreach (["1,000","1,5","12,345","1,234,567","1,2345","1,000,00",",100","1234,567","123,456","-1,000","1,000.5"] as $x) fv_show(filter_var($x, FILTER_VALIDATE_FLOAT, FILTER_FLAG_ALLOW_THOUSAND));
?>
--EXPECT--
f:1.5
f:0.5
f:5
f:1000
f:0.0015
F
F
f:1.5
F
f:1000
F
f:12345
f:1234567
F
F
F
F
f:123456
f:-1000
f:1000.5
