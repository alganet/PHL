--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Simple "$var" interpolation takes "->" only when a LABEL follows
--FILE--
<?php
// php's simple syntax reads "->" as an accessor only when a label START byte
// comes next. With anything else the arrow is literal TEXT and the
// interpolation is just the variable — PHL used to swallow the bare arrow and
// compile a dangling "$o->", which fatalled on source php runs.
set_error_handler(function ($no, $msg) { echo "Warning: $msg\n"; return true; });

class ArrowLabelInterp {
    public $p = 1;
    public $_u = 2;
    public $p9 = 3;
    public function __toString(): string { return 'AL'; }
}

$o = new ArrowLabelInterp();
$a = ['k' => 'v'];

// No label after the arrow: the arrow is literal.
echo "a[$o->1]\n";
echo "b[$o-> p]\n";
echo "c[$o->]\n";
echo "d[$o->{'p'}]\n";
echo "e[$o->-p]\n";
echo "f[$o->\n]\n";

// A real label is still the accessor — including one starting with '_' and one
// carrying digits after the first byte.
echo "g[$o->p]\n";
echo "h[$o->_u]\n";
echo "i[$o->p9]\n";

// The left side keeps its own semantics: a non-object property read warns.
echo "j[$a->k]\n";

// Same rule inside a heredoc.
echo <<<EOT
k[$o->1] l[$o->p] m[$o-> p]
EOT;
echo "\n";
restore_error_handler();
?>
--EXPECT--
a[AL->1]
b[AL-> p]
c[AL->]
d[AL->{'p'}]
e[AL->-p]
f[AL->
]
g[1]
h[2]
i[3]
Warning: Attempt to read property "k" on array
j[]
k[AL->1] l[1] m[AL-> p]
--CLEAN--
<?php
unset($o, $a);
