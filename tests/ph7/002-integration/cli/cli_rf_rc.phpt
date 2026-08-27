--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
CLI --rf/--rc print Reflection exports; unknown names exit 1 with php's Exception line
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$run = function($args) use ($phl) {
    $fp = popen("\"$phl\" $args 2>&1", "r");
    $out = '';
    while (!feof($fp)) { $out .= fgets($fp); }
    $st = pclose($fp);
    return [$out, $st];
};
[$o1, $s1] = $run("--rf strlen");
echo $o1;
[$o2, $s2] = $run("--rf no_such_fn_xyz");
echo $o2, "status-nonzero: ", ($s2 !== 0 ? "yes" : "no"), "\n";
[$o3, $s3] = $run("--rc NoSuchClsXyz");
echo $o3, "status-nonzero: ", ($s3 !== 0 ? "yes" : "no"), "\n";
?>
--EXPECT--
Function [ <internal:Core> function strlen ] {

  - Parameters [1] {
    Parameter #0 [ <required> string $string ]
  }
  - Return [ int ]
}

Exception: Function no_such_fn_xyz() does not exist
status-nonzero: yes
Exception: Class "NoSuchClsXyz" does not exist
status-nonzero: yes
--CLEAN--
<?php
unset($phl, $run, $o1, $s1, $o2, $s2, $o3, $s3);
