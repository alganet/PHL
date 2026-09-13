--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
file_get_contents() of a successfully opened empty file returns "" (FALSE only on open failure)
--FILE--
<?php
$tmp = tempnam(sys_get_temp_dir(), 'fgc');
file_put_contents($tmp, '');
$r = file_get_contents($tmp);
echo var_export($r, true), " len=", strlen($r), "\n";
file_put_contents($tmp, 'data');
echo var_export(file_get_contents($tmp), true), "\n";
unlink($tmp);
echo var_export(@file_get_contents($tmp), true), "\n";
--EXPECT--
'' len=0
'data'
false
