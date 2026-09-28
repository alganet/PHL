--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: goto errors for undefined and unreachable labels
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
function f() {
    goto global_label;
}
global_label:
goto undefined;
undefined_label:
--EXPECTF--
%AFatal error:%A'goto' to undefined label 'global_label'%A
--CLEAN--
<?php

