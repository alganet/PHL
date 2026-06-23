--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var sanitizers: special chars (incl. control bytes), numbers, email, url
--FILE--
<?php
if(!function_exists("fv_show")){function fv_show($v){if($v===true)echo "T";elseif($v===false)echo "F";elseif($v===null)echo "N";elseif(is_int($v))echo "i:$v";elseif(is_float($v))echo "f:".(0+$v);else echo "s:$v";echo "
";}}

fv_show(filter_var("<a>&\"'x", FILTER_SANITIZE_SPECIAL_CHARS));
fv_show(filter_var("x".chr(9)."y".chr(0), FILTER_SANITIZE_SPECIAL_CHARS));
fv_show(filter_var("<a>&\"'x", FILTER_SANITIZE_FULL_SPECIAL_CHARS));
fv_show(filter_var("ab1.5e-3cd", FILTER_SANITIZE_NUMBER_INT));
fv_show(filter_var("ab1.5e-3cd", FILTER_SANITIZE_NUMBER_FLOAT));
fv_show(filter_var("ab1.5e-3cd", FILTER_SANITIZE_NUMBER_FLOAT, FILTER_FLAG_ALLOW_FRACTION|FILTER_FLAG_ALLOW_SCIENTIFIC));
fv_show(filter_var("a b<c>@d.com", FILTER_SANITIZE_EMAIL));
fv_show(filter_var("a <b>\"c\" #d", FILTER_SANITIZE_URL));
?>
--EXPECT--
s:&#60;a&#62;&#38;&#34;&#39;x
s:x&#9;y&#0;
s:&lt;a&gt;&amp;&quot;&#039;x
s:15-3
s:15-3
s:1.5e-3
s:abc@d.com
s:a<b>"c"#d
