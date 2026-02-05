--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_include_path returns current include path
--FILE--
<?php
// get_include_path should return a string (the include path)
$result = get_include_path();
echo is_string($result) ? "ok\n" : "fail\n";
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($result);
