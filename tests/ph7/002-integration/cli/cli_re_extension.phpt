--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
CLI --re prints an extension's Reflection export; an unknown name exits 1
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$run = function ($args) use ($phl) {
    $fp = popen("\"$phl\" $args 2>&1", "r");
    $out = '';
    while (!feof($fp)) { $out .= fgets($fp); }
    $st = pclose($fp);
    return [$out, $st];
};
[$o1, $s1] = $run("--re ctype");
echo preg_replace('/^Extension \[ <persistent> extension #\d+ (\S+) version \S+ \]/',
    'Extension [ <persistent> extension #N $1 version V ]', $o1, 1);
[$o2, $s2] = $run("--re no_such_ext_xyz");
echo $o2, "status-nonzero: ", ($s2 !== 0 ? "yes" : "no"), "\n";
?>
--EXPECTF--
Extension [ <persistent> extension #N ctype version V ] {

  - Functions {
    Function [ <internal:ctype> function ctype_alnum ] {

      - Parameters [1] {
        Parameter #0 [ <required> mixed $text ]
      }
      - Return [ bool ]
    }
%A
  }
}

Exception: Extension "no_such_ext_xyz" does not exist
status-nonzero: yes
--CLEAN--
<?php
unset($phl, $run, $o1, $s1, $o2, $s2);
