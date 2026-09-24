--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var: the three sanitizers PHL never had (string/stripped, encoded, add_slashes) and FILTER_FLAG_EMPTY_STRING_NULL
--FILE--
<?php
/* Three of php's sanitizing filters were a warning and false here: id 513
 * (the "string"/"stripped" filter), FILTER_SANITIZE_ENCODED and
 * FILTER_SANITIZE_ADD_SLASHES. FILTER_FLAG_EMPTY_STRING_NULL, which turns an
 * empty RESULT into null, was undefined beside them.
 *
 * §10: php DEPRECATED the two constant names for filter 513 in 8.1, so this
 * engine does not define them — the filter itself is spellable by id, and by
 * name through filter_id('string'). See the _zend twin. */
function sn($label, $value, $filter, $flags = 0)
{
    printf("%-26s %-22s => %s\n", $label, "'" . addcslashes($value, "\0..\37") . "'",
        var_export(filter_var($value, $filter, $flags), true));
}
$s = "<b>a</b> & 'q' \"d\" `t`";
sn('string', $s, 513);
sn('string no-quotes', $s, 513, FILTER_FLAG_NO_ENCODE_QUOTES);
sn('string encode-amp', $s, 513, FILTER_FLAG_ENCODE_AMP);
sn('string strip-backtick', $s, 513, FILTER_FLAG_STRIP_BACKTICK);
sn('string all-tags', '<b><i></i></b>', 513);
sn('string tag-space', '< notatag', 513);
sn('string low', "a\x01b", 513, FILTER_FLAG_STRIP_LOW);
sn('string encode-low', "a\x01b", 513, FILTER_FLAG_ENCODE_LOW);

sn('encoded', 'a b+c%20d/e?f', FILTER_SANITIZE_ENCODED);
sn('encoded high', "caf\xc3\xa9", FILTER_SANITIZE_ENCODED);
sn('encoded strip-high', "caf\xc3\xa9", FILTER_SANITIZE_ENCODED, FILTER_FLAG_STRIP_HIGH);
sn('encoded strip-low', "a\x01b", FILTER_SANITIZE_ENCODED, FILTER_FLAG_STRIP_LOW);

sn('add_slashes', "sl'a\"sh\\es", FILTER_SANITIZE_ADD_SLASHES);
sn('add_slashes nul', "a\0b", FILTER_SANITIZE_ADD_SLASHES);
sn('add_slashes plain', 'nothing here', FILTER_SANITIZE_ADD_SLASHES);

// FILTER_FLAG_EMPTY_STRING_NULL: an empty RESULT, not only an empty input
sn('raw empty', '', FILTER_DEFAULT);
sn('raw empty null-flag', '', FILTER_DEFAULT, FILTER_FLAG_EMPTY_STRING_NULL);
sn('raw nonempty null-flag', 'x', FILTER_DEFAULT, FILTER_FLAG_EMPTY_STRING_NULL);
sn('string empty null-flag', '', 513, FILTER_FLAG_EMPTY_STRING_NULL);
sn('string all-tags null-flag', '<b></b>', 513, FILTER_FLAG_EMPTY_STRING_NULL);
sn('special empty null-flag', '', FILTER_SANITIZE_SPECIAL_CHARS, FILTER_FLAG_EMPTY_STRING_NULL);
var_dump(FILTER_FLAG_NONE);
?>
--EXPECT--
string                     '<b>a</b> & 'q' "d" `t`' => 'a & &#39;q&#39; &#34;d&#34; `t`'
string no-quotes           '<b>a</b> & 'q' "d" `t`' => 'a & \'q\' "d" `t`'
string encode-amp          '<b>a</b> & 'q' "d" `t`' => 'a &#38; &#39;q&#39; &#34;d&#34; `t`'
string strip-backtick      '<b>a</b> & 'q' "d" `t`' => 'a & &#39;q&#39; &#34;d&#34; t'
string all-tags            '<b><i></i></b>'       => ''
string tag-space           '< notatag'            => ''
string low                 'a\001b'               => 'ab'
string encode-low          'a\001b'               => 'a&#1;b'
encoded                    'a b+c%20d/e?f'        => 'a%20b%2Bc%2520d%2Fe%3Ff'
encoded high               'café'                => 'caf%C3%A9'
encoded strip-high         'café'                => 'caf'
encoded strip-low          'a\001b'               => 'ab'
add_slashes                'sl'a"sh\es'           => 'sl\\\'a\\"sh\\\\es'
add_slashes nul            'a\000b'               => 'a\\0b'
add_slashes plain          'nothing here'         => 'nothing here'
raw empty                  ''                     => ''
raw empty null-flag        ''                     => NULL
raw nonempty null-flag     'x'                    => 'x'
string empty null-flag     ''                     => NULL
string all-tags null-flag  '<b></b>'              => NULL
special empty null-flag    ''                     => ''
int(0)
--CLEAN--
<?php
unset($s);
