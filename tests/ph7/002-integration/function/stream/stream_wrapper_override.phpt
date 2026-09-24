--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: unregistering a BUILT-IN wrapper, replacing it, and restoring it
--FILE--
<?php
set_error_handler(function ($n, $s) {
    if (error_reporting() & $n) { echo "  ERR[$n] $s\n"; }
    return true;
});
class PhlOverrideWrapper {
    public $context;
    private $pos = 0;
    private $path = '';
    public function stream_open($path, $mode, $options, &$opened) {
        $this->path = $path;
        return true;
    }
    public function stream_read($n) {
        if ($this->pos) { return ''; }
        $this->pos = 1;
        return 'FROM-WRAPPER:' . $this->path;
    }
    public function stream_eof() { return $this->pos > 0; }
    public function stream_stat() { return []; }
}
$ov_has = function ($p) { return in_array($p, stream_get_wrappers(), true); };

/* A built-in can be taken out of service, and then it is GONE from the list --
 * both halves of which used to be false here: unregister() only ever handled
 * userland slots and answered false for file/php/data. */
var_dump($ov_has('data'), stream_wrapper_unregister('data'), $ov_has('data'));
var_dump(@file_get_contents('data://text/plain,hi'));

/* The name is free again, which is the point: this is the documented
 * "replace a built-in with my own wrapper" idiom. */
var_dump(stream_wrapper_register('data', 'PhlOverrideWrapper'), $ov_has('data'));
var_dump(file_get_contents('data://whatever'));

/* restore() puts the built-in back and retires the stand-in. */
var_dump(stream_wrapper_restore('data'), $ov_has('data'));
var_dump(file_get_contents('data://text/plain,hi'));

/* php's three answers for restore(): a protocol that was never built in is a
 * warning and FALSE; one that is built in and untouched is an E_NOTICE and
 * TRUE; anything else is restored. */
var_dump(stream_wrapper_restore('phl_no_such_proto'));
var_dump(stream_wrapper_restore('data'));

/* unregister() twice is a warning and false, and the match is case-SENSITIVE
 * even though OPENING a stream folds the scheme. */
var_dump(stream_wrapper_unregister('phl_no_such_proto'));
var_dump(stream_wrapper_unregister('DATA'));
var_dump(stream_wrapper_restore('DATA'));

/* A userland wrapper withdrawn by unregister() also leaves the list, and its
 * name can be registered again afterwards. */
var_dump(stream_wrapper_register('phlov', 'PhlOverrideWrapper'), $ov_has('phlov'));
var_dump(stream_wrapper_unregister('phlov'), $ov_has('phlov'));
var_dump(@file_get_contents('phlov://x'));
var_dump(stream_wrapper_restore('phlov'));
var_dump(stream_wrapper_register('phlov', 'PhlOverrideWrapper'), $ov_has('phlov'));
var_dump(file_get_contents('phlov://x'));
var_dump(stream_wrapper_unregister('phlov'));

/* The headline case: file:// itself. A bare path is the SAME slot, so taking
 * file:// out of service takes the plain open with it -- with php's own two
 * sentences, which are not the ones an unknown scheme gets -- and registering
 * over it routes bare paths through the replacement. */
$ov_file = tempnam(sys_get_temp_dir(), 'phlovf');
file_put_contents($ov_file, "REAL\n");
$ov_mask = function ($v) use ($ov_file) {
    return is_string($v) ? str_replace($ov_file, '<T>', $v) : $v;
};
set_error_handler(function ($n, $s) use ($ov_mask) {
    if (error_reporting() & $n) { echo '  ERR[', $n, '] ', $ov_mask($s), "\n"; }
    return true;
});
var_dump(stream_wrapper_unregister('file'));
var_dump(@file_get_contents($ov_file));
var_dump(stream_wrapper_register('file', 'PhlOverrideWrapper'));
var_dump($ov_mask(file_get_contents($ov_file)));
var_dump(stream_wrapper_restore('file'));
var_dump(file_get_contents($ov_file));
@unlink($ov_file);
?>
--EXPECT--
bool(true)
bool(true)
bool(false)
bool(false)
bool(true)
bool(true)
string(28) "FROM-WRAPPER:data://whatever"
bool(true)
bool(true)
string(2) "hi"
  ERR[2] stream_wrapper_restore(): phl_no_such_proto:// never existed, nothing to restore
bool(false)
  ERR[8] stream_wrapper_restore(): data:// was never changed, nothing to restore
bool(true)
  ERR[2] stream_wrapper_unregister(): Unable to unregister protocol phl_no_such_proto://
bool(false)
  ERR[2] stream_wrapper_unregister(): Unable to unregister protocol DATA://
bool(false)
  ERR[2] stream_wrapper_restore(): DATA:// never existed, nothing to restore
bool(false)
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)
  ERR[2] stream_wrapper_restore(): phlov:// never existed, nothing to restore
bool(false)
bool(true)
bool(true)
string(22) "FROM-WRAPPER:phlov://x"
bool(true)
bool(true)
bool(false)
bool(true)
string(16) "FROM-WRAPPER:<T>"
bool(true)
string(5) "REAL
"
--CLEAN--
<?php
unset($ov_has, $ov_file, $ov_mask);
