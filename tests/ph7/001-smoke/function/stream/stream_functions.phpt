--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: stream_get_contents / wrappers / meta_data / context_create
--FILE--
<?php
$h = fopen("php://memory", "r+");
fwrite($h, "the quick brown fox");
rewind($h);
echo stream_get_contents($h), "\n";
echo stream_get_contents($h, -1, 4), "\n";
echo stream_get_contents($h, 5, 4), "\n";
fclose($h);
$w = stream_get_wrappers();
echo in_array("php", $w) ? "php" : "-", in_array("data", $w) ? "+data" : "-", in_array("file", $w) ? "+file" : "-", "\n";
$m = stream_get_meta_data(fopen("php://memory", "r+"));
echo $m["seekable"] ? "seekable" : "no", "|", $m["timed_out"] ? "T" : "F", "|", $m["blocked"] ? "T" : "F", "\n";
$c = stream_context_create(["http" => ["method" => "GET"]]);
echo $c !== null ? "ctx-ok" : "ctx-null", "\n";
echo stream_get_contents(fopen("data://text/plain,tail-read", "r")), "\n";
?>
--EXPECT--
the quick brown fox
quick brown fox
quick
php+data+file
seekable|F|T
ctx-ok
tail-read
--CLEAN--
<?php
unset($h, $w, $m, $c);
