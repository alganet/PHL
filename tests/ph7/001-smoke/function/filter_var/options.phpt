--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var dispatch: default filter, default option, array input
--FILE--
<?php
if(!function_exists("fv_show")){function fv_show($v){if($v===true)echo "T";elseif($v===false)echo "F";elseif($v===null)echo "N";elseif(is_int($v))echo "i:$v";elseif(is_float($v))echo "f:".(0+$v);else echo "s:$v";echo "
";}}

fv_show(filter_var("hello"));
fv_show(filter_var("keep me", FILTER_DEFAULT));
fv_show(filter_var(["a","b"], FILTER_VALIDATE_INT));
fv_show(filter_var("x", FILTER_VALIDATE_INT, ["options"=>["default"=>-1]]));
fv_show(filter_var("9", FILTER_VALIDATE_INT, ["options"=>["default"=>-1]]));
?>
--EXPECT--
s:hello
s:keep me
F
i:-1
i:9
