--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
CLI populates $argc and the $_SERVER bootstrap entries, matching PHP
--SKIPIF--
<?php if (PHP_OS == 'WINNT') { echo "skip"; } ?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$script = sys_get_temp_dir() . '/phl_srvvars_' . getmypid() . '.php';
file_put_contents($script, <<<'EOF'
<?php
echo "argc=", $argc, "\n";
echo "args=", $argv[1], ",", $argv[2], "\n";
echo "script_name=", basename($_SERVER["SCRIPT_NAME"]), "\n";
echo "php_self_eq=", ($_SERVER["PHP_SELF"] === $_SERVER["SCRIPT_NAME"]) ? "yes" : "no", "\n";
echo "docroot_empty=", ($_SERVER["DOCUMENT_ROOT"] === "") ? "yes" : "no", "\n";
echo "request_time_set=", isset($_SERVER["REQUEST_TIME"]) ? "yes" : "no", "\n";
EOF);
$fp = popen("\"$phl\" \"$script\" alpha beta", 'r');
$out = '';
while (!feof($fp)) { $out .= fgets($fp); }
fclose($fp);
echo $out;
?>
--EXPECTF--
argc=3
args=alpha,beta
script_name=phl_srvvars_%d.php
php_self_eq=yes
docroot_empty=yes
request_time_set=yes
--CLEAN--
<?php
@unlink(sys_get_temp_dir() . '/phl_srvvars_' . getmypid() . '.php');
