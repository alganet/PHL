--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlspecialchars/htmlentities double_encode=false keeps valid entities only
--FILE--
<?php
echo htmlspecialchars("&amp; <a> &bogus; &#39; &eacute;", ENT_QUOTES, "UTF-8", false), "\n";
echo htmlspecialchars("&#233;&#xE9;&#x110000;", ENT_QUOTES, "UTF-8", false), "\n";
echo htmlentities("&amp; & é", ENT_QUOTES, "UTF-8", false), "\n";
echo htmlspecialchars("&amp; &", ENT_QUOTES, "UTF-8", true), "\n"; // default re-encodes
?>
--EXPECT--
&amp; &lt;a&gt; &amp;bogus; &#39; &eacute;
&#233;&#xE9;&amp;#x110000;
&amp; &amp; &eacute;
&amp;amp; &amp;
--CLEAN--
<?php
