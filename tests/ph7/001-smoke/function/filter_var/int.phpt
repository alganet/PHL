--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var FILTER_VALIDATE_INT: base-10, octal/hex flags, overflow, range, passthrough
--FILE--
<?php
if(!function_exists("fv_show")){function fv_show($v){if($v===true)echo "T";elseif($v===false)echo "F";elseif($v===null)echo "N";elseif(is_int($v))echo "i:$v";elseif(is_float($v))echo "f:".(0+$v);else echo "s:$v";echo "
";}}

foreach (["123","+1","-1","0","012","0x1A","1.5","12abc","",". ","+"," 42 ","9223372036854775807","9223372036854775808","-9223372036854775808"] as $x) fv_show(filter_var($x, FILTER_VALIDATE_INT));
fv_show(filter_var("012", FILTER_VALIDATE_INT, FILTER_FLAG_ALLOW_OCTAL));
fv_show(filter_var("0x1A", FILTER_VALIDATE_INT, FILTER_FLAG_ALLOW_HEX));
fv_show(filter_var(123, FILTER_VALIDATE_INT));
fv_show(filter_var(1.0, FILTER_VALIDATE_INT));
fv_show(filter_var(1.5, FILTER_VALIDATE_INT));
fv_show(filter_var(true, FILTER_VALIDATE_INT));
fv_show(filter_var(false, FILTER_VALIDATE_INT));
fv_show(filter_var(null, FILTER_VALIDATE_INT));
fv_show(filter_var(50, FILTER_VALIDATE_INT, ["options"=>["min_range"=>1,"max_range"=>100]]));
fv_show(filter_var(500, FILTER_VALIDATE_INT, ["options"=>["min_range"=>1,"max_range"=>100]]));
fv_show(filter_var("x", FILTER_VALIDATE_INT, FILTER_NULL_ON_FAILURE));
?>
--EXPECT--
i:123
i:1
i:-1
i:0
F
F
F
F
F
F
F
i:42
i:9223372036854775807
F
i:-9223372036854775808
i:10
i:26
i:123
i:1
F
i:1
F
F
i:50
F
N
