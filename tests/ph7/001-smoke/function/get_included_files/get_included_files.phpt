--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_included_files returns an array
--FILE--
<?php
// Create a helper file inside the test directory and include it, to ensure the
// runtime has something to register. The test harness runs tests in a fresh
// environment, so create the file at runtime.
$path = __DIR__ . '/inc_me.php';
file_put_contents($path, "<?php\n// Helper included by get_included_files test\n?>");
include $path;
// The entries vary depending on runtime so assert an array is returned.
echo is_array(get_included_files()) ? "ok\n" : "fail\n";
?>
--EXPECT--
ok
--CLEAN--
<?php
@unlink(__DIR__ . '/inc_me.php');
unset($path);
