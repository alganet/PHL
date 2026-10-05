--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `<?=` block is an echo statement followed by the rest of the block
--DESCRIPTION--
`<?= expr` is `<?php echo expr`, and the block goes on after the echo. It was
compiled as a lone expression and the rest of the block thrown away:
`<?= 2; echo 3 ?>` printed only `2`, `<?= 2` at the end of the input RAN where
php wants its `;`, and a bare `<?=` said `Undefined constant "echo"`. An echo
list that runs into the end of the input names the end of file, not a `;`.
Both doors are asked: a file that OPENS with `<?=` (the first block) and a
`<?=` after inline text (a later block).
--FILE--
<?php
$cases = [
    '<?= 2; echo 3 ?>x',
    '<?= 2; echo 3',
    '<?= 2',
    "<?= 2\n",
    '<?= 2;',
    '<?= 2, 3',
    '<?= 2 3 ?>',
    '<?= 2, ',
    '<?=',
    '<?= ?>',
    'a<?= 1, 2 ?>b<?= "x" . "y" ?>c',
    '<?= 1 ?><?= 2',
    '<?php $a = 1 ?><?= $a; $a++; ?><?= $a ?>',
    '<?php foreach ([1, 2] as $v) { ?>[<?= $v ?>]<?php } ?>',
    '<?php echo',
    "<?php\necho 2,\n\n",
    '<?php echo 2, ?>',
    '<?php echo 2, ;',
];
$file = tempnam(sys_get_temp_dir(), 'phl');
foreach ($cases as $src) {
    foreach (['file' => $src, 'eval' => '?>' . $src] as $door => $code) {
        ob_start();
        try {
            if ($door === 'file') {
                file_put_contents($file, $code);
                include $file;
            } else {
                eval($code);
            }
            $out = ob_get_clean();
            echo $door, ' ', json_encode($src), ": ran, ", json_encode($out), "\n";
        } catch (Throwable $e) {
            ob_end_clean();
            echo $door, ' ', json_encode($src), ": ", get_class($e), ": ", $e->getMessage(), " (line ", $e->getLine(), ")\n";
        }
    }
}
unlink($file);
?>
--EXPECT--
file "<?= 2; echo 3 ?>x": ran, "23x"
eval "<?= 2; echo 3 ?>x": ran, "23x"
file "<?= 2; echo 3": ParseError: syntax error, unexpected end of file, expecting "," or ";" (line 1)
eval "<?= 2; echo 3": ParseError: syntax error, unexpected end of file, expecting "," or ";" (line 1)
file "<?= 2": ParseError: syntax error, unexpected end of file, expecting "," or ";" (line 1)
eval "<?= 2": ParseError: syntax error, unexpected end of file, expecting "," or ";" (line 1)
file "<?= 2\n": ParseError: syntax error, unexpected end of file, expecting "," or ";" (line 2)
eval "<?= 2\n": ParseError: syntax error, unexpected end of file, expecting "," or ";" (line 2)
file "<?= 2;": ran, "2"
eval "<?= 2;": ran, "2"
file "<?= 2, 3": ParseError: syntax error, unexpected end of file, expecting "," or ";" (line 1)
eval "<?= 2, 3": ParseError: syntax error, unexpected end of file, expecting "," or ";" (line 1)
file "<?= 2 3 ?>": ParseError: syntax error, unexpected integer "3", expecting "," or ";" (line 1)
eval "<?= 2 3 ?>": ParseError: syntax error, unexpected integer "3", expecting "," or ";" (line 1)
file "<?= 2, ": ParseError: syntax error, unexpected end of file (line 1)
eval "<?= 2, ": ParseError: syntax error, unexpected end of file (line 1)
file "<?=": ParseError: syntax error, unexpected end of file (line 1)
eval "<?=": ParseError: syntax error, unexpected end of file (line 1)
file "<?= ?>": ParseError: syntax error, unexpected token ";" (line 1)
eval "<?= ?>": ParseError: syntax error, unexpected token ";" (line 1)
file "a<?= 1, 2 ?>b<?= \"x\" . \"y\" ?>c": ran, "a12bxyc"
eval "a<?= 1, 2 ?>b<?= \"x\" . \"y\" ?>c": ran, "a12bxyc"
file "<?= 1 ?><?= 2": ParseError: syntax error, unexpected end of file, expecting "," or ";" (line 1)
eval "<?= 1 ?><?= 2": ParseError: syntax error, unexpected end of file, expecting "," or ";" (line 1)
file "<?php $a = 1 ?><?= $a; $a++; ?><?= $a ?>": ran, "12"
eval "<?php $a = 1 ?><?= $a; $a++; ?><?= $a ?>": ran, "12"
file "<?php foreach ([1, 2] as $v) { ?>[<?= $v ?>]<?php } ?>": ran, "[1][2]"
eval "<?php foreach ([1, 2] as $v) { ?>[<?= $v ?>]<?php } ?>": ran, "[1][2]"
file "<?php echo": ParseError: syntax error, unexpected end of file (line 1)
eval "<?php echo": ParseError: syntax error, unexpected end of file (line 1)
file "<?php\necho 2,\n\n": ParseError: syntax error, unexpected end of file (line 4)
eval "<?php\necho 2,\n\n": ParseError: syntax error, unexpected end of file (line 4)
file "<?php echo 2, ?>": ParseError: syntax error, unexpected token ";" (line 1)
eval "<?php echo 2, ?>": ParseError: syntax error, unexpected token ";" (line 1)
file "<?php echo 2, ;": ParseError: syntax error, unexpected token ";" (line 1)
eval "<?php echo 2, ;": ParseError: syntax error, unexpected token ";" (line 1)
