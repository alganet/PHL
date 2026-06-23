--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
var_export() + __set_state round-trip via eval()
--FILE--
<?php

class VeRound {
    public $a; public $b;
    static function __set_state($d){ $o = new VeRound; $o->a = $d["a"]; $o->b = $d["b"]; return $o; }
}
$o = new VeRound; $o->a = 42; $o->b = ["x", true];
$code = var_export($o, true);
$r = eval("return $code;");
echo $r->a, "|", $r->b[0], "|", ($r->b[1] ? "T" : "F"), "\n";
echo (var_export($r, true) === $code) ? "stable\n" : "DRIFT\n";
?>
--EXPECT--
42|x|T
stable
