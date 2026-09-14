--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the stat family accepts the file:// local-file wrapper scheme
--FILE--
<?php
$f = tempnam(sys_get_temp_dir(), "phl");
file_put_contents($f, "xyz");
var_dump(is_file("file://" . $f));
var_dump(file_exists("file://" . $f));
var_dump(filesize("file://" . $f));
var_dump(is_dir("file://" . dirname($f)));
var_dump(is_file("file://" . $f . "/nope"));
unlink($f);
?>
--EXPECT--
bool(true)
bool(true)
int(3)
bool(true)
bool(false)
--CLEAN--
<?php
