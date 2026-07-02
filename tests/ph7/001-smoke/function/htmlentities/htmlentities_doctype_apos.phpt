--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Single-quote entity per doctype: htmlentities keeps &#039; under XHTML, htmlspecialchars does not
--FILE--
<?php
echo htmlentities("'", ENT_QUOTES), " ",
     htmlentities("'", ENT_QUOTES | ENT_XHTML), " ",
     htmlentities("'", ENT_QUOTES | ENT_HTML5), "\n";
echo htmlspecialchars("'", ENT_QUOTES), " ",
     htmlspecialchars("'", ENT_QUOTES | ENT_XHTML), " ",
     htmlspecialchars("'", ENT_QUOTES | ENT_XML1), " ",
     htmlspecialchars("'", ENT_QUOTES | ENT_HTML5), "\n";
?>
--EXPECT--
&#039; &#039; &apos;
&#039; &apos; &apos; &apos;
--CLEAN--
<?php
