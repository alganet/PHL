--TEST--
The parsing producers' $overrideEncoding names the document's encoding
--FILE--
<?php
function shEnc($l,$fn){ try { $v=$fn(); printf("%-26s => %s\n",$l,var_export($v,true)); }
  catch (\Throwable $e) { printf("%-26s !! %s: %s\n",$l,get_class($e),$e->getMessage()); } }

/* One label per WHATWG encoding NAME, and none of them spelled the way the
 * name is: what the document keeps is the ENCODING the label names, not the
 * label. */
$labels = ['utf8','cp866','latin2','l3','iso88594','cyrillic','arabic','greek',
	'hebrew','logical','latin6','iso8859-13','iso885914','csisolatin9',
	'iso-8859-16','koi','koi8-ru','mac','tis-620','cp1250','cp1251','latin1',
	'cp1253','latin5','cp1255','cp1256','cp1257','cp1258','x-mac-ukrainian',
	'chinese','gb18030','csbig5','x-euc-jp','csiso2022jp','sjis','korean',
	'hz-gb-2312','utf-16be','utf-16','x-user-defined'];
foreach ($labels as $l) {
	shEnc("override '$l'", fn()=>Dom\HTMLDocument::createFromString('<p>x</p>',LIBXML_NOERROR,$l)->charset);
}

/* The three names the encoding answers under, and the XML declaration the
 * same document writes. */
$d = Dom\HTMLDocument::createFromString('<p>x</p>',LIBXML_NOERROR,'iso-8859-1');
shEnc('charset',       fn()=>$d->charset);
shEnc('characterSet',  fn()=>$d->characterSet);
shEnc('inputEncoding', fn()=>$d->inputEncoding);
shEnc('saveXml decl',  fn()=>substr($d->saveXml(),0,strpos($d->saveXml(),'?>')+2));
shEnc('no override',   fn()=>Dom\HTMLDocument::createFromString('<p>x</p>',LIBXML_NOERROR)->charset);
shEnc('null override', fn()=>Dom\HTMLDocument::createFromString('<p>x</p>',LIBXML_NOERROR,null)->charset);

/* createEmpty() does NOT canonicalize: it asks the same table for MEMBERSHIP
 * and keeps the label the program spelled. */
foreach (['iso-8859-1','csibm866','UtF-8'] as $l) {
	shEnc("createEmpty '$l'", fn()=>Dom\HTMLDocument::createEmpty($l)->charset);
}
/* A converter libxml has but WHATWG does not label is refused at both doors. */
foreach (['UCS-4','bogus',''] as $l) {
	shEnc("override '$l'", fn()=>Dom\HTMLDocument::createFromString('<p>x</p>',0,$l)->charset);
}
--EXPECT--
override 'utf8'            => 'UTF-8'
override 'cp866'           => 'IBM866'
override 'latin2'          => 'ISO-8859-2'
override 'l3'              => 'ISO-8859-3'
override 'iso88594'        => 'ISO-8859-4'
override 'cyrillic'        => 'ISO-8859-5'
override 'arabic'          => 'ISO-8859-6'
override 'greek'           => 'ISO-8859-7'
override 'hebrew'          => 'ISO-8859-8'
override 'logical'         => 'ISO-8859-8-I'
override 'latin6'          => 'ISO-8859-10'
override 'iso8859-13'      => 'ISO-8859-13'
override 'iso885914'       => 'ISO-8859-14'
override 'csisolatin9'     => 'ISO-8859-15'
override 'iso-8859-16'     => 'ISO-8859-16'
override 'koi'             => 'KOI8-R'
override 'koi8-ru'         => 'KOI8-U'
override 'mac'             => 'macintosh'
override 'tis-620'         => 'windows-874'
override 'cp1250'          => 'windows-1250'
override 'cp1251'          => 'windows-1251'
override 'latin1'          => 'windows-1252'
override 'cp1253'          => 'windows-1253'
override 'latin5'          => 'windows-1254'
override 'cp1255'          => 'windows-1255'
override 'cp1256'          => 'windows-1256'
override 'cp1257'          => 'windows-1257'
override 'cp1258'          => 'windows-1258'
override 'x-mac-ukrainian' => 'x-mac-cyrillic'
override 'chinese'         => 'GBK'
override 'gb18030'         => 'gb18030'
override 'csbig5'          => 'Big5'
override 'x-euc-jp'        => 'EUC-JP'
override 'csiso2022jp'     => 'ISO-2022-JP'
override 'sjis'            => 'Shift_JIS'
override 'korean'          => 'EUC-KR'
override 'hz-gb-2312'      => 'replacement'
override 'utf-16be'        => 'UTF-16BE'
override 'utf-16'          => 'UTF-16LE'
override 'x-user-defined'  => 'x-user-defined'
charset                    => 'windows-1252'
characterSet               => 'windows-1252'
inputEncoding              => 'windows-1252'
saveXml decl               => '<?xml version="1.0" encoding="windows-1252" standalone="yes"?>'
no override                => 'UTF-8'
null override              => 'UTF-8'
createEmpty 'iso-8859-1'   => 'iso-8859-1'
createEmpty 'csibm866'     => 'csibm866'
createEmpty 'UtF-8'        => 'UtF-8'
override 'UCS-4'           !! ValueError: Dom\HTMLDocument::createFromString(): Argument #3 ($overrideEncoding) must be a valid document encoding
override 'bogus'           !! ValueError: Dom\HTMLDocument::createFromString(): Argument #3 ($overrideEncoding) must be a valid document encoding
override ''                !! ValueError: Dom\HTMLDocument::createFromString(): Argument #3 ($overrideEncoding) must be a valid document encoding
