--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: eval() is a trace frame of its own
--DESCRIPTION--
The other half of backtrace_include_frame.phpt: php gives eval() a frame too, at the
call site and argument-less -- and NAMES the evaluated unit `<file>(<line>) : eval()'d
code`, with its own line numbering, so the frame BELOW the eval carries that name. Ran
under PHL alone until 25 Aug 2026, when the naming shipped; now a parity test.
--FILE--
<?php
function befShow() { echo str_replace([__DIR__ . '/', __DIR__ . '\\'], '', (new Exception)->getTraceAsString()), "\n"; }
eval('befShow();');
function befOuter() { eval('befShow();'); }
befOuter();
?>
--EXPECT--
#0 backtrace_eval_frame.phpt.file(3) : eval()'d code(1): befShow()
#1 backtrace_eval_frame.phpt.file(3): eval()
#2 {main}
#0 backtrace_eval_frame.phpt.file(4) : eval()'d code(1): befShow()
#1 backtrace_eval_frame.phpt.file(4): eval()
#2 backtrace_eval_frame.phpt.file(5): befOuter()
#3 {main}
