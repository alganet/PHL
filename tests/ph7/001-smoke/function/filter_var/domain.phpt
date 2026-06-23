--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var FILTER_VALIDATE_DOMAIN: lenient host check
--FILE--
<?php
if(!function_exists("fv_show")){function fv_show($v){if($v===true)echo "T";elseif($v===false)echo "F";elseif($v===null)echo "N";elseif(is_int($v))echo "i:$v";elseif(is_float($v))echo "f:".(0+$v);else echo "s:$v";echo "
";}}

foreach (["example.com","example",".example.com","example..com","ex@mple.com"] as $x) fv_show(filter_var($x, FILTER_VALIDATE_DOMAIN));
?>
--EXPECT--
s:example.com
s:example
F
F
s:ex@mple.com
