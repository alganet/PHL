--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sha1_file returns known hash for a simple file

--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'phl_sha1_');
file_put_contents($fname, "hello world");
echo sha1_file($fname) . "\n";
unlink($fname);
?>
--EXPECT--
2aae6c35c94fcfb415dbe95f408b9ce91ee846ed
--CLEAN--
<?php
unset($fname);
