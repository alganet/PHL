--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Compile lexer: a '?>' inside a string literal, block comment, attribute string, or heredoc/nowdoc body is not a close tag
--FILE--
<?php
// close-tag sequence inside string literals is not a real close tag
$dq = "a ?" . ">" . " b";
$dq2 = "lit ?".">";
$sq = 'c ?> d';
$re = "/x?>y/";
echo strlen($sq), "|", $sq, "\n";
echo $re, "\n";
echo $dq2, "\n";
$obj = new #[Attr("p?>q")] class { public $v = 7; };
echo $obj->v, "\n";
/* a block comment spans the close-tag bytes and stays a comment */
$h = <<<EOT
here ?> there
EOT;
echo $h, "\n";
$n = <<<'NOW'
raw ?> text
NOW;
echo $n, "\n";
echo "END\n";
--EXPECT--
6|c ?> d
/x?>y/
lit ?>
7
here ?> there
raw ?> text
END
