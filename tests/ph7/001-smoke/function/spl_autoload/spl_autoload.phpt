--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
spl_autoload default autoloader tries lowercased class name with .php extension
--FILE--
<?php
// spl_autoload will try to include "foo.php" — which doesn't exist here.
// We just verify the function is callable and doesn't crash.
spl_autoload('NonExistentClass');
echo "no crash\n";
?>
--EXPECT--
no crash
--CLEAN--
<?php
