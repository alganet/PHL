--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Streams: what every door answers for a handle that is closed, or was never one
--FILE--
<?php
/* Every door of the stream family answers the SAME two refusals: a $stream that
 * is not a resource names the type it got, and one whose device is gone -- an
 * already-closed handle above all -- is "must be an open stream resource".
 * They used to be one legacy warning ("Expecting an IO handle") and a false. */
function t(string $l, callable $f): void {
    try { $r = $f(); } catch (\Throwable $e) { $r = get_class($e) . ': ' . $e->getMessage(); }
    printf("%-26s %s\n", $l, str_replace(["\r", "\n"], ['\r', '\n'], var_export($r, true)));
}
/* The box's php.ini masks E_DEPRECATED; the dir trio's fallback raises one, so
 * both engines have to be asked the same question. */
error_reporting(E_ALL);
set_error_handler(function (int $no, string $msg): bool {
    if (error_reporting() & $no) { printf("  [%d] %s\n", $no, $msg); }
    return true;
});
/* A handle whose device is gone. Nothing below reads its bytes. */
function shut() { $f = fopen('php://memory', 'r+'); fclose($f); return $f; }

$doors = [
    'fclose'              => fn($f) => fclose($f),
    'fread'               => fn($f) => fread($f, 5),
    'fwrite'              => fn($f) => fwrite($f, 'x'),
    'fputs'               => fn($f) => fputs($f, 'x'),
    'fgets'               => fn($f) => fgets($f),
    'fgetc'               => fn($f) => fgetc($f),
    'feof'                => fn($f) => feof($f),
    'ftell'               => fn($f) => ftell($f),
    'fseek'               => fn($f) => fseek($f, 0),
    'rewind'              => fn($f) => rewind($f),
    'fflush'              => fn($f) => fflush($f),
    'fstat'               => fn($f) => fstat($f),
    'ftruncate'           => fn($f) => ftruncate($f, 0),
    'flock'               => fn($f) => flock($f, LOCK_SH),
    'fgetcsv'             => fn($f) => fgetcsv($f),
    'fputcsv'             => fn($f) => fputcsv($f, ['a']),
    'fpassthru'           => fn($f) => fpassthru($f),
    'fprintf'             => fn($f) => fprintf($f, '%s', 'x'),
    'vfprintf'            => fn($f) => vfprintf($f, '%s', ['x']),
    'fscanf'              => fn($f) => fscanf($f, '%s'),
    'stream_get_contents' => fn($f) => stream_get_contents($f),
    'stream_get_meta_data'=> fn($f) => stream_get_meta_data($f),
    'stream_isatty'       => fn($f) => stream_isatty($f),
    'readdir'             => fn($f) => readdir($f),
    'rewinddir'           => fn($f) => rewinddir($f),
    'closedir'            => fn($f) => closedir($f),
    'pclose'              => fn($f) => pclose($f),
    /* The gz* verbs are the SAME bodies under other names, and the refusal has
     * to carry the name that was CALLED. */
    'gzread'              => fn($f) => gzread($f, 5),
    'gzwrite'             => fn($f) => gzwrite($f, 'x'),
    'gzeof'               => fn($f) => gzeof($f),
    'gzclose'             => fn($f) => gzclose($f),
    'gztell'              => fn($f) => gztell($f),
    'gzseek'              => fn($f) => gzseek($f, 0),
    'gzgets'              => fn($f) => gzgets($f),
    'gzgetc'              => fn($f) => gzgetc($f),
    'gzrewind'            => fn($f) => gzrewind($f),
    'gzpassthru'          => fn($f) => gzpassthru($f),
];

echo "-- a handle whose stream was CLOSED\n";
foreach ($doors as $name => $fn) { t($name, fn() => $fn(shut())); }

echo "-- ...and one that was never a resource at all\n";
foreach ([5, 'x', null, 1.5, [1], new stdClass] as $bad) {
    printf("== %s\n", get_debug_type($bad));
    foreach (['fread' => fn($f) => fread($f, 5), 'fclose' => fn($f) => fclose($f),
              'fscanf' => fn($f) => fscanf($f, '%s'), 'readdir' => fn($f) => readdir($f),
              'gzeof' => fn($f) => gzeof($f), 'pclose' => fn($f) => pclose($f)] as $n => $fn) {
        t("  $n", fn() => $fn($bad));
    }
}

echo "-- $stream is refused BEFORE the arguments that follow it\n";
t('vfprintf closed + bad $format', fn() => vfprintf(shut(), [], ['x']));
t('vfprintf closed + bad $values', fn() => vfprintf(shut(), '%s', 'x'));
t('fprintf closed + bad $format',  fn() => fprintf(shut(), []));
t('...and an OPEN one lets them speak', fn() => vfprintf(fopen('php://memory', 'w'), '%s', 'x'));

echo "-- the dir trio has a THIRD case: php's last opened directory stream\n";
t('nothing opened yet',     fn() => readdir());
$d = opendir(__DIR__);
t('readdir(null) finds it', fn() => is_string(readdir(null)));
t('readdir() too',          fn() => is_string(readdir()));
t('rewinddir(null)',        fn() => rewinddir(null));
t('...and it rewound',      fn() => is_string(readdir($d)));
$d2 = opendir(sys_get_temp_dir());
t('a SECOND opendir wins',  fn() => is_string(readdir()));
closedir($d2);
t('closing it drops the fallback', fn() => readdir());
t('closedir() with none',   fn() => closedir());
t('the FIRST handle still reads', fn() => is_string(readdir($d)));
closedir($d);
--EXPECT--
-- a handle whose stream was CLOSED
fclose                     'TypeError: fclose(): Argument #1 ($stream) must be an open stream resource'
fread                      'TypeError: fread(): Argument #1 ($stream) must be an open stream resource'
fwrite                     'TypeError: fwrite(): Argument #1 ($stream) must be an open stream resource'
fputs                      'TypeError: fputs(): Argument #1 ($stream) must be an open stream resource'
fgets                      'TypeError: fgets(): Argument #1 ($stream) must be an open stream resource'
fgetc                      'TypeError: fgetc(): Argument #1 ($stream) must be an open stream resource'
feof                       'TypeError: feof(): Argument #1 ($stream) must be an open stream resource'
ftell                      'TypeError: ftell(): Argument #1 ($stream) must be an open stream resource'
fseek                      'TypeError: fseek(): Argument #1 ($stream) must be an open stream resource'
rewind                     'TypeError: rewind(): Argument #1 ($stream) must be an open stream resource'
fflush                     'TypeError: fflush(): Argument #1 ($stream) must be an open stream resource'
fstat                      'TypeError: fstat(): Argument #1 ($stream) must be an open stream resource'
ftruncate                  'TypeError: ftruncate(): Argument #1 ($stream) must be an open stream resource'
flock                      'TypeError: flock(): Argument #1 ($stream) must be an open stream resource'
fgetcsv                    'TypeError: fgetcsv(): Argument #1 ($stream) must be an open stream resource'
fputcsv                    'TypeError: fputcsv(): Argument #1 ($stream) must be an open stream resource'
fpassthru                  'TypeError: fpassthru(): Argument #1 ($stream) must be an open stream resource'
fprintf                    'TypeError: fprintf(): Argument #1 ($stream) must be an open stream resource'
vfprintf                   'TypeError: vfprintf(): Argument #1 ($stream) must be an open stream resource'
fscanf                     'TypeError: fscanf(): supplied resource is not a valid File-Handle resource'
stream_get_contents        'TypeError: stream_get_contents(): Argument #1 ($stream) must be an open stream resource'
stream_get_meta_data       'TypeError: stream_get_meta_data(): Argument #1 ($stream) must be an open stream resource'
stream_isatty              'TypeError: stream_isatty(): Argument #1 ($stream) must be an open stream resource'
readdir                    'TypeError: readdir(): Argument #1 ($dir_handle) must be an open stream resource'
rewinddir                  'TypeError: rewinddir(): Argument #1 ($dir_handle) must be an open stream resource'
closedir                   'TypeError: closedir(): Argument #1 ($dir_handle) must be an open stream resource'
pclose                     'TypeError: pclose(): Argument #1 ($handle) must be an open stream resource'
gzread                     'TypeError: gzread(): Argument #1 ($stream) must be an open stream resource'
gzwrite                    'TypeError: gzwrite(): Argument #1 ($stream) must be an open stream resource'
gzeof                      'TypeError: gzeof(): Argument #1 ($stream) must be an open stream resource'
gzclose                    'TypeError: gzclose(): Argument #1 ($stream) must be an open stream resource'
gztell                     'TypeError: gztell(): Argument #1 ($stream) must be an open stream resource'
gzseek                     'TypeError: gzseek(): Argument #1 ($stream) must be an open stream resource'
gzgets                     'TypeError: gzgets(): Argument #1 ($stream) must be an open stream resource'
gzgetc                     'TypeError: gzgetc(): Argument #1 ($stream) must be an open stream resource'
gzrewind                   'TypeError: gzrewind(): Argument #1 ($stream) must be an open stream resource'
gzpassthru                 'TypeError: gzpassthru(): Argument #1 ($stream) must be an open stream resource'
-- ...and one that was never a resource at all
== int
  fread                    'TypeError: fread(): Argument #1 ($stream) must be of type resource, int given'
  fclose                   'TypeError: fclose(): Argument #1 ($stream) must be of type resource, int given'
  fscanf                   'TypeError: fscanf(): Argument #1 ($stream) must be of type resource, int given'
  readdir                  'TypeError: readdir(): Argument #1 ($dir_handle) must be of type resource or null, int given'
  gzeof                    'TypeError: gzeof(): Argument #1 ($stream) must be of type resource, int given'
  pclose                   'TypeError: pclose(): Argument #1 ($handle) must be of type resource, int given'
== string
  fread                    'TypeError: fread(): Argument #1 ($stream) must be of type resource, string given'
  fclose                   'TypeError: fclose(): Argument #1 ($stream) must be of type resource, string given'
  fscanf                   'TypeError: fscanf(): Argument #1 ($stream) must be of type resource, string given'
  readdir                  'TypeError: readdir(): Argument #1 ($dir_handle) must be of type resource or null, string given'
  gzeof                    'TypeError: gzeof(): Argument #1 ($stream) must be of type resource, string given'
  pclose                   'TypeError: pclose(): Argument #1 ($handle) must be of type resource, string given'
== null
  fread                    'TypeError: fread(): Argument #1 ($stream) must be of type resource, null given'
  fclose                   'TypeError: fclose(): Argument #1 ($stream) must be of type resource, null given'
  fscanf                   'TypeError: fscanf(): Argument #1 ($stream) must be of type resource, null given'
  [8192] readdir(): Passing null is deprecated, instead the last opened directory stream should be provided
  readdir                  'TypeError: No resource supplied'
  gzeof                    'TypeError: gzeof(): Argument #1 ($stream) must be of type resource, null given'
  pclose                   'TypeError: pclose(): Argument #1 ($handle) must be of type resource, null given'
== float
  fread                    'TypeError: fread(): Argument #1 ($stream) must be of type resource, float given'
  fclose                   'TypeError: fclose(): Argument #1 ($stream) must be of type resource, float given'
  fscanf                   'TypeError: fscanf(): Argument #1 ($stream) must be of type resource, float given'
  readdir                  'TypeError: readdir(): Argument #1 ($dir_handle) must be of type resource or null, float given'
  gzeof                    'TypeError: gzeof(): Argument #1 ($stream) must be of type resource, float given'
  pclose                   'TypeError: pclose(): Argument #1 ($handle) must be of type resource, float given'
== array
  fread                    'TypeError: fread(): Argument #1 ($stream) must be of type resource, array given'
  fclose                   'TypeError: fclose(): Argument #1 ($stream) must be of type resource, array given'
  fscanf                   'TypeError: fscanf(): Argument #1 ($stream) must be of type resource, array given'
  readdir                  'TypeError: readdir(): Argument #1 ($dir_handle) must be of type resource or null, array given'
  gzeof                    'TypeError: gzeof(): Argument #1 ($stream) must be of type resource, array given'
  pclose                   'TypeError: pclose(): Argument #1 ($handle) must be of type resource, array given'
== stdClass
  fread                    'TypeError: fread(): Argument #1 ($stream) must be of type resource, stdClass given'
  fclose                   'TypeError: fclose(): Argument #1 ($stream) must be of type resource, stdClass given'
  fscanf                   'TypeError: fscanf(): Argument #1 ($stream) must be of type resource, stdClass given'
  readdir                  'TypeError: readdir(): Argument #1 ($dir_handle) must be of type resource or null, stdClass given'
  gzeof                    'TypeError: gzeof(): Argument #1 ($stream) must be of type resource, stdClass given'
  pclose                   'TypeError: pclose(): Argument #1 ($handle) must be of type resource, stdClass given'
  [2] Undefined variable $stream
--  is refused BEFORE the arguments that follow it
vfprintf closed + bad $format 'TypeError: vfprintf(): Argument #1 ($stream) must be an open stream resource'
vfprintf closed + bad $values 'TypeError: vfprintf(): Argument #1 ($stream) must be an open stream resource'
fprintf closed + bad $format 'TypeError: fprintf(): Argument #1 ($stream) must be an open stream resource'
...and an OPEN one lets them speak 'TypeError: vfprintf(): Argument #3 ($values) must be of type array, string given'
-- the dir trio has a THIRD case: php's last opened directory stream
  [8192] readdir(): Passing null is deprecated, instead the last opened directory stream should be provided
nothing opened yet         'TypeError: No resource supplied'
  [8192] readdir(): Passing null is deprecated, instead the last opened directory stream should be provided
readdir(null) finds it     true
  [8192] readdir(): Passing null is deprecated, instead the last opened directory stream should be provided
readdir() too              true
  [8192] rewinddir(): Passing null is deprecated, instead the last opened directory stream should be provided
rewinddir(null)            NULL
...and it rewound          true
  [8192] readdir(): Passing null is deprecated, instead the last opened directory stream should be provided
a SECOND opendir wins      true
  [8192] readdir(): Passing null is deprecated, instead the last opened directory stream should be provided
closing it drops the fallback 'TypeError: No resource supplied'
  [8192] closedir(): Passing null is deprecated, instead the last opened directory stream should be provided
closedir() with none       'TypeError: No resource supplied'
the FIRST handle still reads true
