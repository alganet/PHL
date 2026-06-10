--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
die() in a function called from an included file halts the script and runs shutdown
--FILE--
<?php
$target = __DIR__ . '/exit_in_fn_target.php';
file_put_contents($target, "<?php\nfunction inc_die() { die(\"died\\n\"); }\necho \"in_include\\n\";\ninc_die();\necho \"never\\n\";\n");
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
@unlink(__DIR__ . '/exit_in_fn_target.php');
?>
