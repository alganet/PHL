--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Syntax error: Unexpected token ':'
--SKIPIF--
<?php
// Both engines stop at this one refusal; php's bison parser words it differently
// from this recursive-descent one, so the text is pinned under PHL alone.
if (function_exists('zend_version')) { echo 'skip PHL pins the message; php words its parser refusals differently'; }
?>
--FILE--
<?php
echo 1 : 2;
?>
--EXPECTF--
%s Parse error:  Syntax error: Unexpected token ':' %s
--CLEAN--
<?php

