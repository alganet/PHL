--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
const with reserved names
--SKIPIF--
<?php
// Both engines stop at this one refusal and word it identically; php prints a
// `Stack trace:` block under a compile-time FATAL that this engine does not
// (ECOSYSTEM.md F30, behind F6's frame attribution), so the message is pinned
// under PHL alone until that lands.
if (function_exists('zend_version')) { echo 'skip php prints a Stack trace under a compile-time fatal'; }
?>
--FILE--
<?php
const null = 1;
const true = 2;
const false = 3;
echo "Should not reach here\n";
?>
--EXPECTF--
%AFatal error:%ACannot redeclare constant 'null'%A
--CLEAN--
<?php

