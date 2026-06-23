--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var FILTER_VALIDATE_EMAIL: common valid/invalid addresses (incl. 1-char TLD)
--FILE--
<?php
if(!function_exists("fv_show")){function fv_show($v){if($v===true)echo "T";elseif($v===false)echo "F";elseif($v===null)echo "N";elseif(is_int($v))echo "i:$v";elseif(is_float($v))echo "f:".(0+$v);else echo "s:$v";echo "
";}}

foreach (["user+tag@sub.dom.tld","good@example.com","a@b.c","x@y.z","a@b","a b@c.com","a..b@c.com",".x@y.com","x@.com","x@y..com"] as $x) fv_show(filter_var($x, FILTER_VALIDATE_EMAIL));
?>
--EXPECT--
s:user+tag@sub.dom.tld
s:good@example.com
s:a@b.c
s:x@y.z
F
F
F
F
F
F
