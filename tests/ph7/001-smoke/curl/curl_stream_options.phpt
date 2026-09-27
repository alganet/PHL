--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The five options that take a php STREAM, and the two screens they run
--DESCRIPTION--
CURLOPT_FILE, CURLOPT_WRITEHEADER, CURLOPT_STDERR, CURLOPT_INFILE and
CURLOPT_READDATA are the options php hands a php stream rather than a value
libcurl understands. CURLOPT_READDATA and CURLOPT_INFILE are the same number
and the same slot, spelled twice.

php screens them twice over, and the two sentences are different: a value that
is not a resource at all says "supplied argument", a resource the script has
CLOSED says "supplied resource", and both are TypeErrors. The three
DESTINATIONS screen once more, for a handle opened read-only, and that one is a
ValueError -- the read pair does not, because php reads from those.

null is accepted by all five and clears the option, which is how a script puts
the default back: a body that was going to a file goes to the script's output
again.

The write destination is the same ONE setting CURLOPT_RETURNTRANSFER and
CURLOPT_WRITEFUNCTION move, so CURLOPT_FILE joins that race and the last option
set still wins.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
}
?>
--FILE--
<?php
$curlStOpts = array(
    'CURLOPT_FILE' => CURLOPT_FILE,
    'CURLOPT_WRITEHEADER' => CURLOPT_WRITEHEADER,
    'CURLOPT_STDERR' => CURLOPT_STDERR,
    'CURLOPT_INFILE' => CURLOPT_INFILE,
    'CURLOPT_READDATA' => CURLOPT_READDATA,
);
$curlStTmp = tempnam(sys_get_temp_dir(), 'curlst');
file_put_contents($curlStTmp, "a body\n");
$curlStUrl = 'file://' . (DIRECTORY_SEPARATOR === '\\'
    ? '/' . str_replace('\\', '/', $curlStTmp) : $curlStTmp);

$curlStTry = static function ($label, $fn) {
    try {
        printf("  %-16s %s\n", $label, var_export($fn(), true));
    } catch (Throwable $e) {
        printf("  %-16s %s: %s\n", $label, get_class($e), $e->getMessage());
    }
};

foreach ($curlStOpts as $name => $opt) {
    echo $name, "\n";
    $h = curl_init();
    $w = fopen($curlStTmp, 'a');
    $r = fopen($curlStTmp, 'r');
    $closed = fopen($curlStTmp, 'r');
    fclose($closed);
    $curlStTry('writable', static function () use ($h, $opt, $w) {
        return curl_setopt($h, $opt, $w);
    });
    $curlStTry('read-only', static function () use ($h, $opt, $r) {
        return curl_setopt($h, $opt, $r);
    });
    $curlStTry('null', static function () use ($h, $opt) {
        return curl_setopt($h, $opt, null);
    });
    $curlStTry('an int', static function () use ($h, $opt) {
        return curl_setopt($h, $opt, 5);
    });
    $curlStTry('a string', static function () use ($h, $opt) {
        return curl_setopt($h, $opt, 'x');
    });
    $curlStTry('an array', static function () use ($h, $opt) {
        return curl_setopt($h, $opt, array());
    });
    $curlStTry('an object', static function () use ($h, $opt) {
        return curl_setopt($h, $opt, new stdClass());
    });
    $curlStTry('closed', static function () use ($h, $opt, $closed) {
        return curl_setopt($h, $opt, $closed);
    });
    fclose($w);
    fclose($r);
}

/* CURLOPT_FILE takes the body, and answers TRUE rather than the bytes */
$out = tempnam(sys_get_temp_dir(), 'curlout');
$f = fopen($out, 'w');
$h = curl_init($curlStUrl);
curl_setopt($h, CURLOPT_FILE, $f);
printf("to a file: exec=%s\n", var_export(curl_exec($h), true));
fclose($f);
printf("the file has: %s\n", var_export(file_get_contents($out), true));

/* one destination: whichever option was set LAST */
$f = fopen($out, 'w');
$h = curl_init($curlStUrl);
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_FILE, $f);
printf("return then file: %s\n", var_export(curl_exec($h), true));
fclose($f);

$f = fopen($out, 'w');
$h = curl_init($curlStUrl);
curl_setopt($h, CURLOPT_FILE, $f);
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
printf("file then return: %s\n", var_export(curl_exec($h), true));
fclose($f);
printf("the file has: %s\n", var_export(file_get_contents($out), true));

/* null puts the default back: the body goes to the script's output */
$f = fopen($out, 'w');
$h = curl_init($curlStUrl);
curl_setopt($h, CURLOPT_FILE, $f);
curl_setopt($h, CURLOPT_FILE, null);
ob_start();
$r = curl_exec($h);
$printed = ob_get_clean();
fclose($f);
printf("file then null: %s printed=%s\n", var_export($r, true), var_export($printed, true));

unlink($out);
unlink($curlStTmp);
?>
--EXPECT--
CURLOPT_FILE
  writable         true
  read-only        ValueError: curl_setopt(): The provided file handle must be writable
  null             true
  an int           TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  a string         TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  an array         TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  an object        TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  closed           TypeError: curl_setopt(): supplied resource is not a valid File-Handle resource
CURLOPT_WRITEHEADER
  writable         true
  read-only        ValueError: curl_setopt(): The provided file handle must be writable
  null             true
  an int           TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  a string         TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  an array         TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  an object        TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  closed           TypeError: curl_setopt(): supplied resource is not a valid File-Handle resource
CURLOPT_STDERR
  writable         true
  read-only        ValueError: curl_setopt(): The provided file handle must be writable
  null             true
  an int           TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  a string         TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  an array         TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  an object        TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  closed           TypeError: curl_setopt(): supplied resource is not a valid File-Handle resource
CURLOPT_INFILE
  writable         true
  read-only        true
  null             true
  an int           TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  a string         TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  an array         TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  an object        TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  closed           TypeError: curl_setopt(): supplied resource is not a valid File-Handle resource
CURLOPT_READDATA
  writable         true
  read-only        true
  null             true
  an int           TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  a string         TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  an array         TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  an object        TypeError: curl_setopt(): supplied argument is not a valid File-Handle resource
  closed           TypeError: curl_setopt(): supplied resource is not a valid File-Handle resource
to a file: exec=true
the file has: 'a body
'
return then file: true
file then return: 'a body
'
the file has: ''
file then null: true printed='a body
'
--CLEAN--
<?php
unset($curlStOpts, $curlStTmp, $curlStUrl, $curlStTry, $h, $w, $r, $closed, $f, $out, $printed, $name, $opt);
?>
