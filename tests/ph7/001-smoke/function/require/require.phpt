--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
require includes and evaluates a file
--FILE--
<?php
// Create a temp file to require
$dir = sys_get_temp_dir();
$path = $dir . '/ph7_require_test.php';
file_put_contents($path, '<?php echo "included\n";');

require $path;
require $path; // Can require the same file multiple times
?>
--EXPECT--
included
included
--CLEAN--
<?php
@unlink($path);
unset($dir, $path);
