--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
die() inside an included file halts the script but still runs shutdown callbacks
--DESCRIPTION--
Regression: exit/die inside an included file used to hard-exit the C process
(skipping shutdown callbacks and discarding the exit status). It now requests a
VM-wide halt that cascades out of the include, so shutdown callbacks run as in PHP.
--FILE--
<?php
$target = __DIR__ . '/exit_in_include_target.php';
file_put_contents($target, "<?php\necho \"in_include\\n\";\ndie(\"died\\n\");\necho \"never\\n\";\n");
register_shutdown_function(function () { echo "shutdown ran\n"; });
echo "before\n";
include $target;
echo "after_include\n";
?>
--EXPECT--
before
in_include
died
shutdown ran
--CLEAN--
<?php
@unlink(__DIR__ . '/exit_in_include_target.php');
?>
