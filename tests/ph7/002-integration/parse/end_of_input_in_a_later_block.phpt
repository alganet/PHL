--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A PHP block after the first still owes its terminator at the end of the input
--DESCRIPTION--
php reads a closing tag as a statement terminator, so only the block that runs
into the end of the input can leave a statement unfinished. That question was
asked of the FIRST block alone: one opened after a `?>` never learned it had met
the end, so `if (1): ?>A<?php endif` with no `;` RAN where php wants the `;`,
and so did an `endforeach`, or a statement after a `}` closing a block a
previous one opened. A `{` still open at the end is reported as unclosed first,
as it is in the first block.
--FILE--
<?php
foreach ([
    'if (1): ?>A<?php endif',
    'if (1): ?>A<?php endif;',
    'if (1): ?>A<?php endif ?>',
    "if (1): ?>A<?php endif\n\n",
    'echo 1; ?>A<?php echo 2',
    'echo 1; ?>A<?php echo 2 ?>B<?php echo 3',
    'echo 1; ?>A<?php return',
    'echo 1; ?>A<?php $x =',
    'echo 1; ?>A<?php foreach ([1] as $v): ?>B<?php endforeach',
    'if (1) { ?>A<?php echo 1',
    'if (1) { ?>A<?php } echo 1',
    'echo 1; ?>A<?php // c',
    'echo 1; ?>A<?php ;',
] as $src) {
    ob_start();
    try {
        eval($src);
        $out = ob_get_clean();
        echo json_encode($src), ": ran, ", json_encode($out), "\n";
    } catch (ParseError $e) {
        ob_end_clean();
        echo json_encode($src), ": ", $e->getMessage(), " (line ", $e->getLine(), ")\n";
    }
}
?>
--EXPECT--
"if (1): ?>A<?php endif": syntax error, unexpected end of file, expecting ";" (line 1)
"if (1): ?>A<?php endif;": ran, "A"
"if (1): ?>A<?php endif ?>": ran, "A"
"if (1): ?>A<?php endif\n\n": syntax error, unexpected end of file, expecting ";" (line 3)
"echo 1; ?>A<?php echo 2": syntax error, unexpected end of file, expecting "," or ";" (line 1)
"echo 1; ?>A<?php echo 2 ?>B<?php echo 3": syntax error, unexpected end of file, expecting "," or ";" (line 1)
"echo 1; ?>A<?php return": syntax error, unexpected end of file, expecting ";" (line 1)
"echo 1; ?>A<?php $x =": syntax error, unexpected end of file (line 1)
"echo 1; ?>A<?php foreach ([1] as $v): ?>B<?php endforeach": syntax error, unexpected end of file, expecting ";" (line 1)
"if (1) { ?>A<?php echo 1": Unclosed '{' (line 1)
"if (1) { ?>A<?php } echo 1": syntax error, unexpected end of file, expecting "," or ";" (line 1)
"echo 1; ?>A<?php \/\/ c": ran, "1A"
"echo 1; ?>A<?php ;": ran, "1A"
