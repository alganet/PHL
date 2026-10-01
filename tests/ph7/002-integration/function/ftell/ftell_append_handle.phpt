--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ftell() answers the stream layer's own counter, which an append handle parts from
--DESCRIPTION--
php's stream layer positions every handle itself: it asks the descriptor once, as the
stream is created, and from then on moves its own counter by what was read, written or
seeked. ftell() returns that counter and never asks the device again.

A handle opened for APPEND is where the two numbers part company. Every write goes to
the end of the file whatever the descriptor's offset was, so the descriptor jumps to
the end and the counter moves only by the bytes written: after one write to a ten-byte
file php answers 1 and the descriptor is at 11. Reporting the descriptor's offset
instead made ftell() answer the file's size, made a SEEK_CUR seek move from the end of
the file rather than from where the script was, and carried the same error into
ftruncate(), stream_get_contents() and SplFileObject::ftell().

A relative seek is part of the same rule: php resolves SEEK_CUR against its counter and
hands the device an absolute offset, so `fseek($h, 0, SEEK_CUR)` is the no-op it looks
like even on an append handle whose descriptor is somewhere else entirely.
--FILE--
<?php
function t(string $label, $v): void { printf("%-26s %s\n", $label, var_export($v, true)); }

$f = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'phl_ftell_append_' . getmypid() . '.txt';

// A. append: the counter moves by the bytes written, the descriptor does not.
file_put_contents($f, "0123456789");
$h = fopen($f, 'a');
t('open', ftell($h));
fwrite($h, "AB");
t('after 2 appended', ftell($h));
t('seek CUR 0', fseek($h, 0, SEEK_CUR));
t('tell', ftell($h));
t('seek CUR 1', fseek($h, 1, SEEK_CUR));
t('tell', ftell($h));
t('seek END 0', fseek($h, 0, SEEK_END));
t('tell', ftell($h));
fwrite($h, "C");
t('tell', ftell($h));
fclose($h);
t('content', file_get_contents($f));

// B. a+ : reads and writes move the same counter, and a write still appends.
file_put_contents($f, "0123456789");
$h = fopen($f, 'a+');
t('B read 4', fread($h, 4));
t('B tell', ftell($h));
fwrite($h, "XY");
t('B tell', ftell($h));
rewind($h);
t('B rewind tell', ftell($h));
t('B fgets', fgets($h));
t('B tell', ftell($h));
fclose($h);

// C. a+ with a line reader: the read-ahead is discounted, on top of the counter.
file_put_contents($f, "line1\nline2\nline3\n");
$h = fopen($f, 'a+');
t('C fgets', fgets($h));
t('C tell', ftell($h));
$m = stream_get_meta_data($h);
t('C unread_bytes', $m['unread_bytes']);
fwrite($h, "Z");
t('C tell', ftell($h));
fclose($h);

// D. two append handles on one file each count only their own writes.
file_put_contents($f, "0123456789");
$h1 = fopen($f, 'a');
$h2 = fopen($f, 'a');
fwrite($h1, "1");
fwrite($h2, "2");
t('D h1 tell', ftell($h1));
t('D h2 tell', ftell($h2));
fclose($h1);
fclose($h2);
t('D content', file_get_contents($f));

// E. stream_get_contents starts where it is told and leaves the counter there.
file_put_contents($f, "0123456789");
$h = fopen($f, 'a+');
fwrite($h, "AB");
t('E tell', ftell($h));
rewind($h);
t('E contents', stream_get_contents($h));
t('E tell', ftell($h));
t('E from 4', stream_get_contents($h, -1, 4));
t('E tell', ftell($h));
fclose($h);

// F. the same counter through SplFileObject.
file_put_contents($f, "0123456789");
$o = new SplFileObject($f, 'a');
$o->fwrite("Q");
t('F tell', $o->ftell());
unset($o);

// G. a plain r+ handle is unaffected: there the two numbers agree.
file_put_contents($f, "0123456789");
$h = fopen($f, 'r+');
fwrite($h, "ab");
t('G tell', ftell($h));
t('G seek END', fseek($h, 0, SEEK_END));
t('G tell', ftell($h));
fclose($h);

unlink($f);
?>
--EXPECT--
open                       0
after 2 appended           2
seek CUR 0                 0
tell                       2
seek CUR 1                 0
tell                       3
seek END 0                 0
tell                       12
tell                       13
content                    '0123456789ABC'
B read 4                   '0123'
B tell                     4
B tell                     6
B rewind tell              0
B fgets                    '0123456789XY'
B tell                     12
C fgets                    'line1
'
C tell                     6
C unread_bytes             12
C tell                     7
D h1 tell                  1
D h2 tell                  1
D content                  '012345678912'
E tell                     2
E contents                 '0123456789AB'
E tell                     12
E from 4                   '456789AB'
E tell                     12
F tell                     1
G tell                     2
G seek END                 0
G tell                     10
