--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php://input is a stream, and under a command line it is an empty one
--DESCRIPTION--
`fopen('php://input')` was `Invalid php:// URL specified` here -- the one php://
name the engine did not have, and the one any framework reads a JSON or form body
through. php's input stream is the REQUEST BODY, and a command line has none: php
CLI answers an empty stream rather than reading standard input (what
`php x.php < file` supplies arrives through STDIN, and php://input stays ""). It
runs on the memory machinery, so it is seekable and can be read twice, and it
differs from php://memory in exactly four answers -- `fflush()` is FALSE (the one
stream in the family whose flush op fails), `fstat()` is FALSE, `ftruncate()` is
unsupported, and its metadata names it `Input`.
--FILE--
<?php
function pisShow($pisLabel, $pisVal) {
    if (is_array($pisVal)) { $pisVal = 'array(' . count($pisVal) . ')'; }
    else { $pisVal = var_export($pisVal, true); }
    echo str_pad($pisLabel, 24), ' => ', $pisVal, "\n";
}
set_error_handler(function ($pisNo, $pisMsg) { echo "  [$pisNo] $pisMsg\n"; return true; });

pisShow('file_get_contents', file_get_contents('php://input'));
pisShow('again', file_get_contents('php://input'));

$pisH = fopen('php://input', 'r');
pisShow('is a stream', $pisH !== false);
$pisM = stream_get_meta_data($pisH);
foreach (['wrapper_type', 'stream_type', 'mode', 'unread_bytes', 'seekable', 'uri', 'eof'] as $pisK) {
    pisShow("meta $pisK", $pisM[$pisK]);
}
pisShow('fread', fread($pisH, 5));
pisShow('ftell', ftell($pisH));
pisShow('feof', feof($pisH));
pisShow('rewind', rewind($pisH));
pisShow('stream_get_contents', stream_get_contents($pisH));
pisShow('fseek end', fseek($pisH, 0, SEEK_END));
pisShow('ftell', ftell($pisH));
pisShow('fwrite', fwrite($pisH, 'zz'));
pisShow('fflush', fflush($pisH));
pisShow('fstat', fstat($pisH));
pisShow('ftruncate', ftruncate($pisH, 0));
fclose($pisH);

/* Every mode opens the same read-only stream, and its metadata says so. */
foreach (['r', 'rb', 'w', 'r+'] as $pisMode) {
    $pisX = fopen('php://input', $pisMode);
    pisShow("open '$pisMode'", stream_get_meta_data($pisX)['mode']);
    fclose($pisX);
}
pisShow('opendir', opendir('php://input'));

/* A write nothing can take is FALSE from every door, and silent: the notice
 * that names an errno comes from the stdio device, and this one is not it. */
pisShow('file_put_contents', file_put_contents('php://input', 'q'));
$pisSrc = sys_get_temp_dir() . '/phl_pis_' . getmypid();
file_put_contents($pisSrc, 'hello');
pisShow('copy to input', copy($pisSrc, 'php://input'));
@unlink($pisSrc);

/* Every php:// name but `temp` matches WHOLE: php has no `php://memoryx`. */
foreach (['php://memory', 'php://memoryx', 'php://memory/', 'php://temp',
          'php://tempx', 'php://temp/maxmemory:1024', 'php://input',
          'php://inputx', 'php://input/', 'php://output', 'php://outputx',
          'php://bogus', 'php://'] as $pisName) {
    $pisX = @fopen($pisName, 'r');
    pisShow("open $pisName", $pisX !== false);
    if ($pisX !== false) { fclose($pisX); }
}
restore_error_handler();
--EXPECT--
file_get_contents        => ''
again                    => ''
is a stream              => true
meta wrapper_type        => 'PHP'
meta stream_type         => 'Input'
meta mode                => 'rb'
meta unread_bytes        => 0
meta seekable            => true
meta uri                 => 'php://input'
meta eof                 => false
fread                    => ''
ftell                    => 0
feof                     => true
rewind                   => true
stream_get_contents      => ''
fseek end                => 0
ftell                    => 0
fwrite                   => false
fflush                   => false
fstat                    => false
  [2] ftruncate(): Can't truncate this stream!
ftruncate                => false
open 'r'                 => 'rb'
open 'rb'                => 'rb'
open 'w'                 => 'rb'
open 'r+'                => 'rb'
  [2] opendir(php://input): Failed to open directory: not implemented
opendir                  => false
file_put_contents        => false
copy to input            => false
open php://memory        => true
  [2] fopen(): Invalid php:// URL specified
  [2] fopen(php://memoryx): Failed to open stream: operation failed
open php://memoryx       => false
  [2] fopen(): Invalid php:// URL specified
  [2] fopen(php://memory/): Failed to open stream: operation failed
open php://memory/       => false
open php://temp          => true
open php://tempx         => true
open php://temp/maxmemory:1024 => true
open php://input         => true
  [2] fopen(): Invalid php:// URL specified
  [2] fopen(php://inputx): Failed to open stream: operation failed
open php://inputx        => false
  [2] fopen(): Invalid php:// URL specified
  [2] fopen(php://input/): Failed to open stream: operation failed
open php://input/        => false
open php://output        => true
  [2] fopen(): Invalid php:// URL specified
  [2] fopen(php://outputx): Failed to open stream: operation failed
open php://outputx       => false
  [2] fopen(): Invalid php:// URL specified
  [2] fopen(php://bogus): Failed to open stream: operation failed
open php://bogus         => false
  [2] fopen(): Invalid php:// URL specified
  [2] fopen(php://): Failed to open stream: operation failed
open php://              => false
