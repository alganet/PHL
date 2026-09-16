--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Identifiers carrying high bytes (UTF-8 names, legal in php) lex correctly -- regression for the keyword-hash out-of-bounds read
--FILE--
<?php
$café = 1;
$café++;
echo $café, "\n";

function ação($x) { return $x * 2; }
echo ação(21), "\n";

class Ünicode { const Grüße = 'hallo'; public $mañana = 'tomorrow'; }
$o = new Ünicode();
echo Ünicode::Grüße, "\n";
echo $o->mañana, "\n";
?>
--EXPECT--
2
42
hallo
tomorrow
--CLEAN--
<?php
unset($o);
