--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unserialize(): malformed input returns false
--FILE--
<?php

set_error_handler(function($n, $s){ return true; }); // swallow PHP's unserialize warnings
foreach (["","x","i:12","i:9z;","s:5:\"ab\";","s:2:\"abc\";","a:1:{i:0;}",
          "a:2:{i:0;i:1;}","O:99:\"Nope\":0:{}","N","b:2;","r:1;"] as $s)
    echo (unserialize($s) === false) ? "F\n" : "V\n";
restore_error_handler();
?>
--EXPECT--
F
F
F
F
F
F
F
F
F
F
F
F
