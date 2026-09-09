--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
version_compare canonicalizes and orders versions (dev<alpha<beta<RC<release<pl)
--FILE--
<?php
$pairs = [["1.0.0","1.0.0"],["1.0","1.0.0"],["1.0","1.0.1"],["8.2.0","8.5.7"],
  ["1.0.0-dev","1.0.0"],["1.0.0-alpha","1.0.0-beta"],["1.0.0-rc1","1.0.0"],
  ["1.0.0pl1","1.0.0"],["2.0","1.9.9"],["1.0a","1.0b"],["1.0RC1","1.0RC2"],
  ["7.4","8.0"],["","1.0"],["1","1.0.0"],["1.0.0beta2","1.0.0beta10"]];
$out = [];
foreach ($pairs as [$a, $b]) {
    $out[] = "$a|$b=" . var_export(version_compare($a, $b), true)
        . "," . var_export(version_compare($a, $b, ">="), true)
        . "," . var_export(version_compare($a, $b, "lt"), true);
}
$out[] = "ext:" . (extension_loaded("json") ? "1" : "0") . (extension_loaded("pcre") ? "1" : "0") . (extension_loaded("SPL") ? "1" : "0");
echo implode("\n", $out), "\n";
--EXPECT--
1.0.0|1.0.0=0,true,false
1.0|1.0.0=-1,false,true
1.0|1.0.1=-1,false,true
8.2.0|8.5.7=-1,false,true
1.0.0-dev|1.0.0=-1,false,true
1.0.0-alpha|1.0.0-beta=-1,false,true
1.0.0-rc1|1.0.0=-1,false,true
1.0.0pl1|1.0.0=1,true,false
2.0|1.9.9=1,true,false
1.0a|1.0b=-1,false,true
1.0RC1|1.0RC2=-1,false,true
7.4|8.0=-1,false,true
|1.0=-1,false,true
1|1.0.0=-1,false,true
1.0.0beta2|1.0.0beta10=-1,false,true
ext:111
--CLEAN--
<?php
