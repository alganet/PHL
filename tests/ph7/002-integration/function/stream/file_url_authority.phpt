--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: the file:// URL's authority and its slash run
--SKIPIF--
<?php
// The URL forms below are spelled with a POSIX absolute path. On Windows the
// same shapes carry a drive letter, which php's own locate_url_wrapper lets
// through a different branch and leaves a different byte string behind — a
// platform difference inside php, not a divergence, so it is not asserted with
// one expectation for both.
if (DIRECTORY_SEPARATOR === '\\') { echo 'skip POSIX file:// URL spelling'; }
?>
--FILE--
<?php
$fu_dir  = sys_get_temp_dir();
$fu_name = 'phl_fileurl_' . getmypid() . '.txt';
$fu_path = $fu_dir . '/' . $fu_name;
file_put_contents($fu_path, "OK\n");
/* The temp path carries a pid; mask it so the expectation is the SHAPE. */
set_error_handler(function ($n, $s) use ($fu_path, $fu_name) {
    echo '  ERR[', $n, '] ',
        str_replace([$fu_path, ltrim($fu_path, '/'), $fu_name], '<P>', $s), "\n";
    return true;
});

/* Everything php reaches through file:// : an empty authority, `localhost`
 * (in any case, and only with its own slash), and any number of slashes after
 * either — the run collapses to one. */
foreach (['file://', 'file:///', 'file:////', 'file://localhost',
          'file://LOCALHOST', 'file://localhost/', 'file://localhost//'] as $fu_pre) {
    printf("%-20s => %s\n", $fu_pre . '...',
        var_export(@file_get_contents($fu_pre . ltrim($fu_path, '/')), true));
}

/* `file://` with nothing after it is the root DIRECTORY, not a file called
 * "file://" — which is what PHL used to look for. */
var_dump(is_dir('file://'), is_dir('file:///'));

/* And the authority php will not reach. The path that follows is NOT opened
 * relative to the cwd, which is what PHL used to do with it. */
chdir($fu_dir);
var_dump(@file_get_contents('file://' . $fu_name));
var_dump(@file_get_contents($fu_name));

@unlink($fu_path);
?>
--EXPECT--
  ERR[2] file_get_contents(): Remote host file access not supported, file:/<P>
  ERR[2] file_get_contents(file:/<P>): Failed to open stream: no suitable wrapper could be found
file://...           => false
file:///...          => 'OK
'
file:////...         => 'OK
'
  ERR[2] file_get_contents(): Remote host file access not supported, file://localhost<P>
  ERR[2] file_get_contents(file://localhost<P>): Failed to open stream: no suitable wrapper could be found
file://localhost...  => false
  ERR[2] file_get_contents(): Remote host file access not supported, file://LOCALHOST<P>
  ERR[2] file_get_contents(file://LOCALHOST<P>): Failed to open stream: no suitable wrapper could be found
file://LOCALHOST...  => false
file://localhost/... => 'OK
'
file://localhost//... => 'OK
'
bool(true)
bool(true)
  ERR[2] file_get_contents(): Remote host file access not supported, file://<P>
  ERR[2] file_get_contents(file://<P>): Failed to open stream: no suitable wrapper could be found
bool(false)
string(3) "OK
"
--CLEAN--
<?php
unset($fu_dir, $fu_name, $fu_path, $fu_pre);
