--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An include/require/eval is a trace frame of its own
--DESCRIPTION--
ECOSYSTEM.md F6's third companion. php shows the construct that loaded a unit as a
frame between that unit's frames and the caller's -- `#1 main.php(2): include('...')`
-- and nothing in this engine recorded one: an include shares its caller's variable
scope, so it pushes no frame to be found later. The activation is tracked explicitly
now.

Two rules ride with it, both php's: the frame's file and line are the CALL SITE (so a
require written inside a function names that function's file), and the argument -- the
unit being loaded, which php prints elided as `'...'` -- is carried by every such
frame EXCEPT the innermost one of a trace.
--FILE--
<?php
require __DIR__ . '/backtrace_include_frame.inc';
function bifOuter() { require __DIR__ . '/backtrace_include_frame2.inc'; }
bifOuter();
?>
--EXPECT--
top: #0 backtrace_include_frame.phpt.file(2): require() | #1 {main}
#0 backtrace_include_frame.inc(4): bifShow()
#1 backtrace_include_frame.phpt.file(2): require('...')
#2 {main}
top: #0 backtrace_include_frame.phpt.file(3): require() | #1 backtrace_include_frame.phpt.file(4): bifOuter() | #2 {main}
#0 backtrace_include_frame2.inc(3): bifShow()
#1 backtrace_include_frame.phpt.file(3): require('...')
#2 backtrace_include_frame.phpt.file(4): bifOuter()
#3 {main}
