--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_decode() holds numbers to JSON's grammar: no leading zeros, no bare '.', no empty exponent
--FILE--
<?php
$bad = ['[01]', '[00]', '[-01]', '[0123]', '[1.]', '[5.]', '[1e]', '[1e+]', '[1.2e]'];
foreach ($bad as $doc) {
    var_dump(json_decode($doc, true));
    echo json_last_error(), "\n";
}
$good = ['[0]', '[-0]', '[0.5]', '[0e1]', '[1E2]', '[1e+2]', '[1e-2]', '[1.0e5]', '[-12.75]'];
foreach ($good as $doc) {
    echo json_encode(json_decode($doc, true)), " ", json_last_error(), "\n";
}
// json_validate() answers from the same grammar.
var_dump(json_validate('[01]'), json_last_error(), json_validate('[0.5e+3]'));
?>
--EXPECT--
NULL
4
NULL
4
NULL
4
NULL
4
NULL
4
NULL
4
NULL
4
NULL
4
NULL
4
[0] 0
[0] 0
[0.5] 0
[0] 0
[100] 0
[100] 0
[0.01] 0
[100000] 0
[-12.75] 0
bool(false)
int(4)
bool(true)
--CLEAN--
<?php
