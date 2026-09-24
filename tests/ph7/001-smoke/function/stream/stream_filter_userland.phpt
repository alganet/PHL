--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: a filter written in PHP — stream_filter_register and the buckets
--DESCRIPTION--
stream_filter_register() was a Call to undefined function, so a filter could
only ever be one of the ones this engine ships. A userland filter is a CLASS:
php instantiates it once per attachment, tells it the name it was created under
and the params it was given, and then calls filter($in,$out,&$consumed,$closing)
with two BRIGADE handles. The script walks $in with stream_bucket_make_writeable
— which hands over one bucket at a time as a StreamBucket whose `data` it may
replace outright — and appends what it made to $out. What it RETURNS is the
chain's answer, and the closing call is where a tail made with
stream_bucket_new() gets out.
--FILE--
<?php
$sfnShow = function ($label, $fn) {
    $seen = [];
    set_error_handler(function ($n, $m) use (&$seen) { $seen[] = $m; return true; });
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    restore_error_handler();
    echo $label, ' => ', str_replace("\n", '', $out);
    foreach ($seen as $m) { echo ' | ', $m; }
    echo "\n";
};
$sfnRun = function ($data, $name, $params = null, $mode = STREAM_FILTER_READ) {
    $h = fopen('php://memory', 'w+');
    fwrite($h, $data);
    rewind($h);
    $r = $params === null
        ? stream_filter_append($h, $name, $mode)
        : stream_filter_append($h, $name, $mode, $params);
    $out = $r === false ? false : stream_get_contents($h);
    fclose($h);
    return $out;
};

/* The plainest one: take each bucket, change it, put it back. */
class SfnUpper extends php_user_filter {
    public function filter($in, $out, &$consumed, bool $closing): int {
        while ($b = stream_bucket_make_writeable($in)) {
            $b->data = strtoupper($b->data);
            $consumed += $b->datalen;
            stream_bucket_append($out, $b);
        }
        return PSFS_PASS_ON;
    }
}
$sfnShow('register', fn() => stream_filter_register('sfn.up', 'SfnUpper'));
$sfnShow('again', fn() => stream_filter_register('sfn.up', 'SfnUpper'));
$sfnShow('it is listed', fn() => in_array('sfn.up', stream_get_filters()));
$sfnShow('read through it', fn() => $sfnRun('hello world', 'sfn.up'));
$sfnShow('write through it', function () use ($sfnRun) {
    $h = fopen('php://memory', 'w+');
    stream_filter_append($h, 'sfn.up', STREAM_FILTER_WRITE);
    fwrite($h, 'written');
    rewind($h);
    $s = stream_get_contents($h);
    fclose($h);
    return $s;
});

/* What the instance is TOLD, and when. php sets `stream` for the duration of
 * the call and no longer, so onCreate() sees nothing there. */
class SfnTell extends php_user_filter {
    public static $log = [];
    public function onCreate(): bool {
        self::$log[] = ['onCreate', $this->filtername, $this->params,
                        is_resource($this->stream ?? null)];
        return true;
    }
    public function onClose(): void { self::$log[] = ['onClose']; }
    public function filter($in, $out, &$consumed, bool $closing): int {
        self::$log[] = ['filter', $closing, $consumed, is_resource($this->stream)];
        while ($b = stream_bucket_make_writeable($in)) {
            $consumed += $b->datalen;
            stream_bucket_append($out, $b);
        }
        return PSFS_PASS_ON;
    }
}
stream_filter_register('sfn.tell', 'SfnTell');
$sfnShow('what it is told', function () use ($sfnRun) {
    SfnTell::$log = [];
    $s = $sfnRun('ab', 'sfn.tell', ['k' => 1]);
    return [$s, SfnTell::$log];
});
$sfnShow('no params is NULL', function () use ($sfnRun) {
    SfnTell::$log = [];
    $sfnRun('ab', 'sfn.tell');
    return SfnTell::$log[0];
});

/* A wildcard registration is told the name it was ASKED for. */
class SfnName extends php_user_filter {
    public function filter($in, $out, &$consumed, bool $closing): int {
        while ($b = stream_bucket_make_writeable($in)) {
            $b->data = $this->filtername;
            $consumed += $b->datalen;
            stream_bucket_append($out, $b);
        }
        return PSFS_PASS_ON;
    }
}
stream_filter_register('sfn.w.*', 'SfnName');
$sfnShow('wildcard', fn() => $sfnRun('x', 'sfn.w.anything'));
$sfnShow('wildcard misses', fn() => @$sfnRun('x', 'sfn.z.anything'));

/* The closing call is the only chance to emit a tail, and stream_bucket_new is
 * the only way to make one. */
class SfnTail extends php_user_filter {
    public function filter($in, $out, &$consumed, bool $closing): int {
        while ($b = stream_bucket_make_writeable($in)) {
            $consumed += $b->datalen;
            stream_bucket_append($out, $b);
        }
        if ($closing) {
            stream_bucket_append($out, stream_bucket_new($this->stream, '[END]'));
        }
        return PSFS_PASS_ON;
    }
}
stream_filter_register('sfn.tail', 'SfnTail');
$sfnShow('a tail of its own', fn() => $sfnRun('body', 'sfn.tail'));
$sfnShow('prepend puts it first', function () use ($sfnRun) {
    return $sfnRun('body', 'sfn.pre');
});
class SfnPre extends php_user_filter {
    public function filter($in, $out, &$consumed, bool $closing): int {
        while ($b = stream_bucket_make_writeable($in)) {
            $consumed += $b->datalen;
            stream_bucket_append($out, $b);
            stream_bucket_prepend($out, stream_bucket_new($this->stream, '<'));
        }
        return PSFS_PASS_ON;
    }
}
stream_filter_register('sfn.pre', 'SfnPre');
$sfnShow('prepend', fn() => $sfnRun('body', 'sfn.pre'));

/* The three answers. FEED_ME swallows what it took; ERR_FATAL with the input
 * still on the brigade is php's loud failure and a FALSE read. */
class SfnFeed extends php_user_filter {
    public function filter($in, $out, &$consumed, bool $closing): int {
        while ($b = stream_bucket_make_writeable($in)) { $consumed += $b->datalen; }
        return PSFS_FEED_ME;
    }
}
class SfnBad extends php_user_filter {
    public function filter($in, $out, &$consumed, bool $closing): int {
        return PSFS_ERR_FATAL;
    }
}
stream_filter_register('sfn.feed', 'SfnFeed');
stream_filter_register('sfn.bad', 'SfnBad');
$sfnShow('feed me', fn() => $sfnRun('abc', 'sfn.feed'));
$sfnShow('fatal with input left', function () use ($sfnRun) {
    $h = fopen('php://memory', 'w+');
    fwrite($h, 'abc');
    rewind($h);
    stream_filter_append($h, 'sfn.bad', STREAM_FILTER_READ);
    $r = fread($h, 3);
    fclose($h);
    return $r;
});

/* onCreate answering FALSE is php's refusal, and a class that is not there is
 * the other one — the attach says so twice. */
class SfnNo extends php_user_filter {
    public function onCreate(): bool { return false; }
    public function filter($in, $out, &$consumed, bool $closing): int { return PSFS_PASS_ON; }
}
stream_filter_register('sfn.no', 'SfnNo');
stream_filter_register('sfn.missing', 'SfnNoSuchClassAnywhere');
$sfnShow('onCreate refuses', fn() => $sfnRun('x', 'sfn.no'));
$sfnShow('class not defined', fn() => $sfnRun('x', 'sfn.missing'));

/* A subclass that overrides nothing gets php's own do-nothing filter(), which
 * takes no bucket and answers no PASS_ON — so the read fails loudly. */
class SfnDefault extends php_user_filter {}
stream_filter_register('sfn.default', 'SfnDefault');
$sfnShow('overriding nothing', fn() => $sfnRun('abc', 'sfn.default'));

/* Registering refuses an empty name or class. */
$sfnShow('empty name', fn() => stream_filter_register('', 'SfnUpper'));
$sfnShow('empty class', fn() => stream_filter_register('sfn.e', ''));

/* A name one of the built-ins answers to is taken. */
$sfnShow('a built-in name', fn() => stream_filter_register('string.toupper', 'SfnUpper'));

/* A handle or a bucket kept past the call it belonged to. php keeps both alive;
 * the brigade behind the handle is what is gone. Walking that handle again reads
 * freed stack memory in php (a 4 GiB allocation on macOS), so it is not asked. */
class SfnKeep extends php_user_filter {
    public static $in = null, $bucket = null;
    public function filter($in, $out, &$consumed, bool $closing): int {
        if (self::$in === null) { self::$in = $in; }
        while ($b = stream_bucket_make_writeable($in)) {
            self::$bucket = $b;
            $consumed += $b->datalen;
            stream_bucket_append($out, $b);
        }
        return PSFS_PASS_ON;
    }
}
stream_filter_register('sfn.keep', 'SfnKeep');
$sfnShow('kept past the call', function () use ($sfnRun) {
    SfnKeep::$in = SfnKeep::$bucket = null;
    $s = $sfnRun('abcd', 'sfn.keep');
    return [$s, get_resource_type(SfnKeep::$bucket->bucket), SfnKeep::$bucket->data,
            get_resource_type(SfnKeep::$in)];
});
$sfnShow('a bucket of its own outside a call', function () {
    $h = fopen('php://memory', 'w+');
    $b = stream_bucket_new($h, 'zz');
    fclose($h);
    return [$b->data, $b->datalen, get_resource_type($b->bucket)];
});

/* The constants a filter answers with. */
$sfnShow('PSFS', fn() => [PSFS_PASS_ON, PSFS_FEED_ME, PSFS_ERR_FATAL,
    PSFS_FLAG_NORMAL, PSFS_FLAG_FLUSH_INC, PSFS_FLAG_FLUSH_CLOSE]);
$sfnShow('the class shape', fn() => [
    get_class_methods('php_user_filter'),
    array_keys(get_class_vars('php_user_filter')),
    array_keys(get_class_vars('StreamBucket')),
]);
?>
--EXPECT--
register => true
again => false
it is listed => true
read through it => 'HELLO WORLD'
write through it => 'WRITTEN'
what it is told => array (  0 => 'ab',  1 =>   array (    0 =>     array (      0 => 'onCreate',      1 => 'sfn.tell',      2 =>       array (        'k' => 1,      ),      3 => false,    ),    1 =>     array (      0 => 'filter',      1 => false,      2 => NULL,      3 => true,    ),    2 =>     array (      0 => 'filter',      1 => true,      2 => NULL,      3 => true,    ),    3 =>     array (      0 => 'onClose',    ),  ),)
no params is NULL => array (  0 => 'onCreate',  1 => 'sfn.tell',  2 => NULL,  3 => false,)
wildcard => 'sfn.w.anything'
wildcard misses => false | stream_filter_append(): Unable to locate filter "sfn.z.anything"
a tail of its own => 'body[END]'
prepend puts it first => false | stream_filter_append(): Unable to locate filter "sfn.pre"
prepend => '<body'
feed me => ''
fatal with input left => false | fread(): Unprocessed filter buckets remaining on input brigade
onCreate refuses => false | stream_filter_append(): Unable to create or locate filter "sfn.no"
class not defined => false | stream_filter_append(): User-filter "sfn.missing" requires class "SfnNoSuchClassAnywhere", but that class is not defined | stream_filter_append(): Unable to create or locate filter "sfn.missing"
overriding nothing => '' | stream_get_contents(): Unprocessed filter buckets remaining on input brigade
empty name => ValueError: stream_filter_register(): Argument #1 ($filter_name) must be a non-empty string
empty class => ValueError: stream_filter_register(): Argument #2 ($class) must be a non-empty string
a built-in name => false
kept past the call => array (  0 => 'abcd',  1 => 'userfilter.bucket',  2 => 'abcd',  3 => 'userfilter.bucket brigade',)
a bucket of its own outside a call => array (  0 => 'zz',  1 => 2,  2 => 'userfilter.bucket',)
PSFS => array (  0 => 2,  1 => 1,  2 => 0,  3 => 0,  4 => 1,  5 => 2,)
the class shape => array (  0 =>   array (    0 => 'filter',    1 => 'onCreate',    2 => 'onClose',  ),  1 =>   array (    0 => 'filtername',    1 => 'params',    2 => 'stream',  ),  2 =>   array (    0 => 'bucket',    1 => 'data',    2 => 'datalen',    3 => 'dataLength',  ),)
