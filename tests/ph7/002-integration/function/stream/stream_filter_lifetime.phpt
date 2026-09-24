--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: a filter does not outlive the handle it was attached to
--DESCRIPTION--
Closing a stream ends every filter on it — the write chain gets its closing call
while the device is still open, and the filter RESOURCE is dead afterwards. This
is the rule on the close paths that are not fclose(): pclose() and closedir().
The pipe's consumer is `cat` or, where there is none, `more`, which appends a
line ending of its own — hence the trim().
--SKIPIF--
<?php if (!function_exists('popen')) die('skip no popen'); ?>
--FILE--
<?php
$out = tempnam(sys_get_temp_dir(), 'sfl');
$cat = DIRECTORY_SEPARATOR === '\\' ? 'more' : 'cat';

/* pclose(): the pipe's write chain runs before the pipe is torn down. */
$p = popen($cat . ' > ' . escapeshellarg($out), 'w');
$f = stream_filter_append($p, 'string.toupper', STREAM_FILTER_WRITE);
fwrite($p, 'piped through');
pclose($p);
var_dump(is_resource($f));
var_dump(trim(file_get_contents($out)));

/* closedir(): a directory handle takes one too, and loses it the same way. */
$d = opendir(sys_get_temp_dir());
$g = @stream_filter_append($d, 'string.toupper');
closedir($d);
var_dump(is_resource($g));

@unlink($out);
?>
--EXPECT--
bool(false)
string(13) "PIPED THROUGH"
bool(false)
