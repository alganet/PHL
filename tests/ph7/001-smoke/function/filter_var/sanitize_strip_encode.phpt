--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var STRIP_LOW/HIGH/BACKTICK + ENCODE_LOW/HIGH/AMP flags on UNSAFE_RAW and SPECIAL_CHARS
--FILE--
<?php
if(!function_exists("h")){function h($v){ echo bin2hex($v),"\n"; }}
$IN = "a\x09b`c\x7Fd\xC3\xA9e&f";  // a TAB b ` c DEL d é(2 bytes) e & f
// FILTER_UNSAFE_RAW applies the strip/encode flags
h(filter_var($IN, FILTER_UNSAFE_RAW, ["flags"=>FILTER_FLAG_STRIP_LOW]));       // drop <32
h(filter_var($IN, FILTER_UNSAFE_RAW, ["flags"=>FILTER_FLAG_STRIP_HIGH]));      // drop >=127 (incl DEL, é)
h(filter_var($IN, FILTER_UNSAFE_RAW, ["flags"=>FILTER_FLAG_STRIP_BACKTICK]));  // drop `
h(filter_var($IN, FILTER_UNSAFE_RAW, ["flags"=>FILTER_FLAG_ENCODE_LOW]));      // <32 -> &#N;
h(filter_var($IN, FILTER_UNSAFE_RAW, ["flags"=>FILTER_FLAG_ENCODE_HIGH]));     // each >=127 byte -> &#N;
h(filter_var($IN, FILTER_UNSAFE_RAW, ["flags"=>FILTER_FLAG_ENCODE_AMP]));      // & -> &#38;
// strip wins over encode for the same range
h(filter_var($IN, FILTER_UNSAFE_RAW, ["flags"=>FILTER_FLAG_STRIP_LOW|FILTER_FLAG_ENCODE_LOW]));
// no flags -> passthrough (UNSAFE_RAW == DEFAULT)
h(filter_var($IN, FILTER_UNSAFE_RAW));
h(filter_var($IN, FILTER_DEFAULT));
// SPECIAL_CHARS layers strip/encode over its numeric-entity encoding
h(filter_var($IN, FILTER_SANITIZE_SPECIAL_CHARS));
h(filter_var($IN, FILTER_SANITIZE_SPECIAL_CHARS, ["flags"=>FILTER_FLAG_STRIP_HIGH]));
h(filter_var($IN, FILTER_SANITIZE_SPECIAL_CHARS, ["flags"=>FILTER_FLAG_STRIP_LOW]));
h(filter_var($IN, FILTER_SANITIZE_SPECIAL_CHARS, ["flags"=>FILTER_FLAG_ENCODE_HIGH]));
?>
--EXPECT--
616260637f64c3a9652666
610962606364652666
610962637f64c3a9652666
612623393b6260637f64c3a9652666
610962606326233132373b6426233139353b26233136393b652666
61096260637f64c3a965262333383b66
616260637f64c3a9652666
61096260637f64c3a9652666
61096260637f64c3a9652666
612623393b6260637f64c3a965262333383b66
612623393b6260636465262333383b66
616260637f64c3a965262333383b66
612623393b62606326233132373b6426233139353b26233136393b65262333383b66
