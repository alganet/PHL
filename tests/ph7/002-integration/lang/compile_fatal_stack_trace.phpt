--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A compile-time fatal prints php's Stack trace, and which frames it has
--DESCRIPTION--
The half of ECOSYSTEM.md F30 that had to wait for F6: php prints a `Stack trace:`
block under a compile-time FATAL (and none under a parse error, which is its
parser's own refusal). Which frames it holds is php's three phases, and they answer
differently -- so the trace is not one thing to print but three:

  - a class REDECLARATION is php's RUN-time refusal: it cannot early-bind a name it
    already holds, so DECLARE_CLASS reports it and the require that loaded the unit
    IS on the trace;
  - every other compiler refusal is raised BEFORE php pushes that activation, so the
    require is NOT on it;
  - a MODIFIER-run refusal is the parser's, raised before any op array exists, and
    php prints no trace at all.
--FILE--
<?php
function cfstRun($file) {
    $out = shell_exec(escapeshellarg(PHP_BINARY) . ' ' . escapeshellarg(__DIR__ . '/' . $file) . ' 2>&1');
    echo str_replace([__DIR__ . '/', __DIR__ . '\\'], '', $out), "---\n";
}
file_put_contents(__DIR__ . '/cfst_run1.php', "<?php\nfunction w(){ require __DIR__.'/compile_fatal_stack_trace.inc'; }\nw();\n");
file_put_contents(__DIR__ . '/cfst_run2.php', "<?php\nfunction w(){ require __DIR__.'/compile_fatal_stack_trace2.inc'; }\nw();\n");
file_put_contents(__DIR__ . '/cfst_run3.php', "<?php\nfunction w(){ require __DIR__.'/cfst_mod.inc'; }\nw();\n");
file_put_contents(__DIR__ . '/cfst_mod.inc', "<?php\nfinal abstract class CfstFA {}\n");
cfstRun('cfst_run1.php');
cfstRun('cfst_run2.php');
cfstRun('cfst_run3.php');
?>
--EXPECTF--
%AFatal error:  Cannot redeclare class CfstDup%Ain compile_fatal_stack_trace.inc on line 5
Stack trace:
#0 cfst_run1.php(2): require()
#1 cfst_run1.php(3): w()
#2 {main}
---
%AFatal error:  Abstract function CfstAbs::m() cannot contain body in compile_fatal_stack_trace2.inc on line 4
Stack trace:
#0 cfst_run2.php(3): w()
#1 {main}
---
%AFatal error:  Cannot use the final modifier on an abstract class in cfst_mod.inc on line 2
---
--CLEAN--
<?php
@unlink(__DIR__ . '/cfst_run1.php');
@unlink(__DIR__ . '/cfst_run2.php');
@unlink(__DIR__ . '/cfst_run3.php');
@unlink(__DIR__ . '/cfst_mod.inc');
