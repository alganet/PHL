--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
set_include_path() replaces the include_path and returns the previous value (subprocess: mutates global state)
--FILE--
<?php
// Normalize a known baseline first (the default include_path differs per engine/ini).
set_include_path('.');
echo "base=", get_include_path(), "\n";
$sipOld = set_include_path('/foo:/bar/baz');
echo "old=", $sipOld, "\n";
echo "new=", get_include_path(), "\n";
$sipOld2 = set_include_path('/single');
echo "old2=", $sipOld2, "\n";
echo "cur=", get_include_path(), "\n";
?>
--EXPECT--
base=.
old=.
new=/foo:/bar/baz
old2=/foo:/bar/baz
cur=/single
--CLEAN--
<?php
unset($sipOld, $sipOld2);
