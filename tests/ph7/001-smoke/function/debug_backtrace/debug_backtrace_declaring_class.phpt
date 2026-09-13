--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
debug_backtrace() 'class' is the method's declaring class; static frames carry class/type
--DESCRIPTION--
Regression: the backtrace 'class' reported the runtime $this class rather than
the DECLARING class of the executing method, so an inherited method called on a
subclass showed the subclass. It now reports the class the method is defined in
(the frame's function metadata), matching php. Static-method frames also get
'class'/'type' ('::') now, which the old $this-only path dropped.
--FILE--
<?php
class DbdcBase {
    public function run() { return debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS)[0]; }
    public static function srun() { return debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS)[0]; }
}
class DbdcChild extends DbdcBase {}
$f = (new DbdcChild)->run();
echo "inst class={$f['class']} type={$f['type']}\n";
$s = DbdcChild::srun();
echo "static class={$s['class']} type={$s['type']}\n";
?>
--EXPECT--
inst class=DbdcBase type=->
static class=DbdcBase type=::
--CLEAN--
<?php
