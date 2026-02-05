--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strip_tags with allowed tags triggers AddTag/FindTag internal logic
--FILE--
<?php
echo strip_tags('<p>Hello</p>', '<p>') . "\n";
echo strip_tags('<p>Hello<script>alert(1)</script></p>', '<p>') . "\n";
echo strip_tags('<P>Hi</P>', '<p>') . "\n"; // case-insensitive tag matching
// With allowed tag containing whitespace/EOF characters
echo strip_tags('<div>OK</div>', ' <div> ') . "\n";
?>
--EXPECT--
<p>Hello</p>
<p>Helloalert(1)</p>
<P>Hi</P>
<div>OK</div>
--CLEAN--
<?php

