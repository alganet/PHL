--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var FILTER_VALIDATE_URL: scheme and host required
--FILE--
<?php
if(!function_exists("fv_show")){function fv_show($v){if($v===true)echo "T";elseif($v===false)echo "F";elseif($v===null)echo "N";elseif(is_int($v))echo "i:$v";elseif(is_float($v))echo "f:".(0+$v);else echo "s:$v";echo "
";}}

foreach (["http://example.com","https://u:p@h.com:8/p?q#f","ftp://x","http://example","//x.com","http://","notaurl"] as $x) fv_show(filter_var($x, FILTER_VALIDATE_URL));
?>
--EXPECT--
s:http://example.com
s:https://u:p@h.com:8/p?q#f
s:ftp://x
s:http://example
F
F
F
