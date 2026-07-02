--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlentities is UTF-8 aware: named entities for mapped codepoints, raw passthrough otherwise
--FILE--
<?php
echo htmlentities("héllo wörld — “q” €"), "\n";
echo htmlentities("あ日本語 é"), "\n";
echo htmlentities("é", ENT_QUOTES | ENT_XML1), "\n";
?>
--EXPECT--
h&eacute;llo w&ouml;rld &mdash; &ldquo;q&rdquo; &euro;
あ日本語 &eacute;
é
--CLEAN--
<?php
