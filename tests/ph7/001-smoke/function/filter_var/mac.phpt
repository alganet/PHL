--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var FILTER_VALIDATE_MAC: colon/dash hex formats
--FILE--
<?php
if(!function_exists("fv_show")){function fv_show($v){if($v===true)echo "T";elseif($v===false)echo "F";elseif($v===null)echo "N";elseif(is_int($v))echo "i:$v";elseif(is_float($v))echo "f:".(0+$v);else echo "s:$v";echo "
";}}

foreach (["01:23:45:67:89:ab","01-23-45-67-89-ab","0123456789ab","01:23:45:67:89:gg","01:23:45:67:89"] as $x) fv_show(filter_var($x, FILTER_VALIDATE_MAC));
?>
--EXPECT--
s:01:23:45:67:89:ab
s:01-23-45-67-89-ab
F
F
F
