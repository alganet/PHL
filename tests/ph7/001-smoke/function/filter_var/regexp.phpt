--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var FILTER_VALIDATE_REGEXP: matches the supplied pattern
--FILE--
<?php
if(!function_exists("fv_show")){function fv_show($v){if($v===true)echo "T";elseif($v===false)echo "F";elseif($v===null)echo "N";elseif(is_int($v))echo "i:$v";elseif(is_float($v))echo "f:".(0+$v);else echo "s:$v";echo "
";}}

fv_show(filter_var("abc123", FILTER_VALIDATE_REGEXP, ["options"=>["regexp"=>"/^[a-z]+[0-9]+$/"]]));
fv_show(filter_var("ABC", FILTER_VALIDATE_REGEXP, ["options"=>["regexp"=>"/^[a-z]+$/"]]));
?>
--EXPECT--
s:abc123
F
