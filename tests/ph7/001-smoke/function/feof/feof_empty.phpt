--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test feof() function with empty files
--FILE--
<?php

// An empty file is not at EOF until a read has come back empty: php's flag is
// set after the fact, and feof() itself never reads. (This test used to pin
// PHL's own answer, with a SKIPIF saying zend "handles feof on empty files
// differently"; both engines agree now.)
$emptyFile = tempnam(sys_get_temp_dir(), 'ph7_empty_test');
file_put_contents($emptyFile, '');

$fp = fopen($emptyFile, 'r');
echo feof($fp) ? "EOF before reading\n" : "not at EOF before reading\n";
$data = fread($fp, 8192);
echo var_export($data, true), "\n";
echo feof($fp) ? "EOF after reading\n" : "not at EOF after reading\n";
fclose($fp);
unlink($emptyFile);

?>
--EXPECT--
not at EOF before reading
''
EOF after reading
--CLEAN--
<?php
// Cleanup handled in test
unset($emptyFile, $fp, $data);
