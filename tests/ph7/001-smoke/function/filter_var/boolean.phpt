--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var FILTER_VALIDATE_BOOLEAN: recognized true/false sets and null-on-failure
--FILE--
<?php
if(!function_exists("fv_show")){function fv_show($v){if($v===true)echo "T";elseif($v===false)echo "F";elseif($v===null)echo "N";elseif(is_int($v))echo "i:$v";elseif(is_float($v))echo "f:".(0+$v);else echo "s:$v";echo "
";}}

foreach (["1","true","On","YES","0","false","off","no",". ","maybe"] as $x) fv_show(filter_var($x, FILTER_VALIDATE_BOOLEAN));
fv_show(filter_var("maybe", FILTER_VALIDATE_BOOLEAN, FILTER_NULL_ON_FAILURE));
fv_show(filter_var("0", FILTER_VALIDATE_BOOLEAN, FILTER_NULL_ON_FAILURE));
fv_show(filter_var(1, FILTER_VALIDATE_BOOLEAN));
fv_show(filter_var(1.5, FILTER_VALIDATE_BOOLEAN));
?>
--EXPECT--
T
T
T
T
F
F
F
F
F
F
N
F
T
F
