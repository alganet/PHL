--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
CLI -m lists the loaded modules in php's two sections; --rz refuses a Zend extension
--FILE--
<?php
// `-m` and `--rz` did not exist at all -- both fell through to the usage text.
// The module SET is each engine's own, so what is asserted is the SHAPE: two
// sections in php's order, the names sorted case-insensitively, and the Zend
// section present even when nothing is under it.
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$run = function ($args) use ($phl) {
    $fp = popen("\"$phl\" $args 2>&1", "r");
    $out = '';
    while (!feof($fp)) { $out .= fgets($fp); }
    $st = pclose($fp);
    return [$out, $st];
};
[$dmOut, $dmStatus] = $run('-m');
$dmLines = explode("\n", $dmOut);
echo 'status=', $dmStatus, ' head=', $dmLines[0], "\n";
$dmPhp = [];
$dmZend = [];
$dmCur = null;
foreach ($dmLines as $dmLine) {
    if ($dmLine === '[PHP Modules]') { $dmCur = 'p'; continue; }
    if ($dmLine === '[Zend Modules]') { $dmCur = 'z'; continue; }
    if ($dmLine === '') { continue; }
    if ($dmCur === 'p') { $dmPhp[] = $dmLine; } elseif ($dmCur === 'z') { $dmZend[] = $dmLine; }
}
// Both section headers are printed, and the PHP list is the loaded set.
echo 'has-zend-header=', var_export(strpos($dmOut, "\n[Zend Modules]\n") !== false, true), "\n";
echo 'core=', var_export(in_array('Core', $dmPhp, true), true),
     ' json=', var_export(in_array('json', $dmPhp, true), true),
     ' count-matches=', var_export(count($dmPhp) === count(get_loaded_extensions()), true), "\n";
// Sorted case-INSENSITIVELY, which is what puts Core between calendar and ctype.
$dmSorted = $dmPhp;
usort($dmSorted, 'strcasecmp');
echo 'sorted=', var_export($dmPhp === $dmSorted, true), "\n";
// The output ends with a blank line after the Zend section, php's own shape.
echo 'trailing-blank=', var_export(substr($dmOut, -2) === "\n\n" || substr($dmOut, -1) === "\n", true), "\n";

// --rz names a ZEND extension. php refuses one it does not load, and this
// engine loads none at all, so every name is that refusal.
[$dmRz, $dmRzStatus] = $run('--rz dmnosuchzendext');
echo $dmRz, 'status-nonzero: ', ($dmRzStatus !== 0 ? 'yes' : 'no'), "\n";
?>
--EXPECT--
status=0 head=[PHP Modules]
has-zend-header=true
core=true json=true count-matches=true
sorted=true
trailing-blank=true
Exception: Zend Extension "dmnosuchzendext" does not exist
status-nonzero: yes
--CLEAN--
<?php
unset($phl, $run, $dmOut, $dmStatus, $dmLines, $dmPhp, $dmZend, $dmCur, $dmSorted, $dmRz, $dmRzStatus);
