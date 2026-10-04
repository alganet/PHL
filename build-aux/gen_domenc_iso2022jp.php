<?php
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Cuts src/ph7/vm_dom_iso2022jp.h -- the HTML document's ISO-2022-JP index --
 * from php's own HTML decoder and encoder.
 *
 *   XDEBUG_MODE=off php build-aux/gen_domenc_iso2022jp.php \
 *       > src/ph7/vm_dom_iso2022jp.h
 *
 * The oracle is `Dom\HTMLDocument` and nothing else. iconv and mbstring each
 * carry an ISO-2022-JP of their own and neither is this one.
 *
 * ISO-2022-JP is why gen_domenc_dbcs.php and its four siblings all refuse it:
 * a byte means nothing by itself here. An ESCAPE SEQUENCE selects a code set
 * and every byte after it is read in that set until the next escape, so the
 * STATES and the escapes between them have to be named before a cell can be.
 * Nothing about them is assumed:
 *
 *   - which ESC x y are escapes at all is swept over the whole 95x95 of
 *     printable second and third bytes, and which state each selects is read
 *     off what the bytes behind it then answer;
 *   - whether any escape is longer than two bytes is swept the same way over
 *     ESC $ ( x -- php recognises NONE, so this encoding has no JIS X 0212;
 *   - what each single-byte state answers is swept byte by byte rather than
 *     assumed to be ASCII, which is how the two bytes Roman redefines and the
 *     three every state refuses were found;
 *   - which bytes open a PAIR in the two-byte state, and which are accepted
 *     behind one, are swept rather than taken from the standard's 0x21..0x7E.
 *
 * The sweep runs through a `<style>` element per cell, not a comma-separated
 * text node: ISO-2022-JP passes ASCII through to the tokenizer, and an escape
 * that php REFUSES re-emits its own bytes, so a payload can forge any
 * separator -- `<`, `&` and `,` included. `<style>` is RAWTEXT, where the only
 * byte sequence with a meaning is `</style`, which no two-byte payload can
 * spell. The decoder's state carries across the element boundary, so every
 * payload ends by shifting back to ASCII.
 *
 * ENCODE is swept through a COMMENT node, which saveHtml() writes without
 * escaping, so the bytes are the encoder's. The encoder is stateful too, and
 * its state is readable in the sweep: every mapped character comes back framed
 * by the shift into the two-byte set and the shift back out of it.
 */
const LABEL = 'ISO-2022-JP';
const RESET = "\x1b(B";                 /* ESC ( B -- back to the ASCII set */
const ESC_JIS = "\x1b\x24\x42";         /* ESC $ B -- the two-byte set */
const ESC_ROMAN = "\x1b\x28\x4A";       /* ESC ( J */
const ESC_KATA = "\x1b\x28\x49";        /* ESC ( I */
const L0 = 0x21, L1 = 0x7E;             /* the pair window this file asserts */
const NROW = L1 - L0 + 1, NCOL = L1 - L0 + 1;

function die_(string $s): void { fwrite(STDERR,LABEL.": $s\n"); exit(1); }
/*
 * One <style> per payload. The count is checked: a payload that closed the
 * element early would silently shift every answer after it.
 */
function cells(array $pay): array {
	$src = '<!doctype html>';
	foreach( $pay as $s ){ $src .= "<style>$s</style>"; }
	$d = @Dom\HTMLDocument::createFromString($src,LIBXML_NOERROR,LABEL);
	$l = $d->getElementsByTagName('style');
	if( $l->count() !== count($pay) ){
		die_($l->count().' style elements for '.count($pay).' payloads');
	}
	$o = [];
	foreach( $l as $e ){ $o[] = $e->textContent; }
	return $o;
}
function cps(string $s): array {
	$o = []; $n = mb_strlen($s,'UTF-8');
	for( $i = 0 ; $i < $n ; $i++ ){ $o[] = mb_ord(mb_substr($s,$i,1,'UTF-8'),'UTF-8'); }
	return $o;
}
/* --- which ESC x y are escapes, and which state each selects ------------- */
/* An escape that php refuses answers U+FFFD and then re-reads both its bytes
 * as characters of the state it was already in, so a refusal is exactly the
 * three-character answer `FFFD x y` -- anything else is an escape. */
$pay = []; $k = [];
for( $x = 0x20 ; $x <= 0x7E ; $x++ ){
	for( $y = 0x20 ; $y <= 0x7E ; $y++ ){
		if( $x === 0x2F && $y === 0x73 ) continue;   /* "/s" would open </style */
		$pay[] = "\x1b".chr($x).chr($y)."\x21\x21".RESET;
		$k[] = [$x,$y];
	}
}
$esc = [];
foreach( cells($pay) as $i => $t ){
	[$x,$y] = $k[$i];
	$c = cps($t);
	if( $c === [0xFFFD,$x,$y,0x21,0x21] ) continue;
	$esc[sprintf('%02X%02X',$x,$y)] = $c;
}
/* Each state is named by what the two 0x21 bytes behind its escape answered:
 * one character for a PAIR set, two identical for a single-byte set. */
$want = [
	'2440' => [0x3000],                 /* ESC $ @ -- the two-byte set */
	'2442' => [0x3000],                 /* ESC $ B -- the same set */
	'2842' => [0x21,0x21],              /* ESC ( B -- ASCII */
	'284A' => [0x21,0x21],              /* ESC ( J -- Roman */
	'2849' => [0xFF61,0xFF61],          /* ESC ( I -- half-width katakana */
];
$a = array_keys($esc); $b = array_keys($want); sort($a); sort($b);
if( $a !== $b ){
	die_('escapes are '.implode(' ',$a).', expected '.implode(' ',$b));
}
foreach( $want as $key => $v ){
	if( $esc[$key] !== $v ){
		die_("escape $key answered ".implode(' ',$esc[$key]).' for its two 0x21 bytes');
	}
}
/* --- and no escape is longer than two bytes ------------------------------ */
$pay = []; $k = [];
for( $x = 0x20 ; $x <= 0x7E ; $x++ ){ $pay[] = "\x1b\x24\x28".chr($x)."\x21\x21".RESET; $k[] = $x; }
foreach( cells($pay) as $i => $t ){
	if( cps($t) !== [0xFFFD,0x24,0x28,$k[$i],0x21,0x21] ){
		die_(sprintf('ESC $ ( %02X is a third escape byte',$k[$i]));
	}
}
/* --- what each single-byte state answers, byte by byte ------------------- */
/* 0x0A and 0x0D are left out: the tokenizer normalises both to U+000A after
 * the decoder has run, so this door cannot see what the decoder made of them.
 * 0x1B opens an escape in every state and is measured as one. */
$law = [
	'ASCII' => [RESET,     function(int $b){ return $b < 0x80 ? $b : 0xFFFD; }],
	'Roman' => [ESC_ROMAN, function(int $b){
		if( $b === 0x5C ) return 0x00A5;
		if( $b === 0x7E ) return 0x203E;
		return $b < 0x80 ? $b : 0xFFFD;
	}],
	'Kata'  => [ESC_KATA,  function(int $b){
		return ($b >= 0x21 && $b <= 0x5F) ? 0xFF61 + $b - 0x21 : 0xFFFD;
	}],
];
$refuse = [0x0E,0x0F];                  /* the bytes every state refuses */
foreach( $law as $nm => [$e,$fn] ){
	$pay = []; $k = [];
	for( $b = 0x01 ; $b <= 0xFF ; $b++ ){
		if( $b === 0x0A || $b === 0x0D || $b === 0x1B ) continue;
		$pay[] = $e.chr($b).RESET; $k[] = $b;
	}
	foreach( cells($pay) as $i => $t ){
		$b = $k[$i];
		$exp = in_array($b,$refuse,true) ? 0xFFFD : $fn($b);
		if( cps($t) !== [$exp] ){
			die_(sprintf('%s state: byte %02X answered %s, not U+%04X',
				$nm,$b,implode(' ',cps($t)),$exp));
		}
	}
}
/* --- which bytes open a pair in the two-byte state ----------------------- */
/* A lead eats the 0x21 behind it and answers ONE character; a byte that is no
 * lead answers U+FFFD and leaves the 0x21 to be read as a lead of its own,
 * which then runs out of input -- two characters either way. */
$pay = []; $k = [];
for( $b = 0x01 ; $b <= 0xFF ; $b++ ){
	if( $b === 0x0A || $b === 0x0D || $b === 0x1B ) continue;
	$pay[] = ESC_JIS.chr($b)."\x21".RESET; $k[] = $b;
}
$lead = [];
foreach( cells($pay) as $i => $t ){
	if( count(cps($t)) === 1 ){ $lead[] = $k[$i]; }
}
if( $lead !== range(L0,L1) ){
	die_(sprintf('the pair leads are %02X..%02X and %d of them, not %02X..%02X',
		$lead[0],end($lead),count($lead),L0,L1));
}
/* --- and which bytes are accepted behind one ---------------------------- */
/* Every byte but the escape is: a trail either names a cell or answers U+FFFD,
 * and never hands itself back. 0x1B is the one byte that aborts the pair. */
$pay = []; $k = [];
for( $b = 0x01 ; $b <= 0xFF ; $b++ ){ if( $b === 0x1B ) continue; $pay[] = ESC_JIS."\x21".chr($b).RESET; $k[] = $b; }
foreach( cells($pay) as $i => $t ){
	if( count(cps($t)) !== 1 ){
		die_(sprintf('trail %02X answered %d characters',$k[$i],count(cps($t))));
	}
}
if( cps(cells([ESC_JIS."\x21".RESET."\x21"])[0]) !== [0xFFFD,0x21] ){
	die_('an escape behind a lead does not abort the pair');
}
/* --- the pairs ---------------------------------------------------------- */
$cell = array_fill(0,NROW * NCOL,0);
$n = 0;
for( $L = L0 ; $L <= L1 ; $L++ ){
	$pay = []; $k = [];
	for( $T = L0 ; $T <= L1 ; $T++ ){ $pay[] = ESC_JIS.chr($L).chr($T).RESET; $k[] = $T; }
	foreach( cells($pay) as $i => $t ){
		$c = cps($t);
		if( count($c) !== 1 ){ die_(sprintf('cell %02X%02X is not one character',$L,$k[$i])); }
		if( $c[0] === 0xFFFD ) continue;            /* no character here */
		if( $c[0] > 0xFFFF ){
			die_(sprintf('cell %02X%02X is outside the BMP (U+%04X)',$L,$k[$i],$c[0]));
		}
		$cell[($L - L0) * NCOL + $k[$i] - L0] = $c[0]; $n++;
	}
}
/* --- the encoder, swept over the BMP ------------------------------------ */
/* The separator is an ASCII character, and the reason is the encoder's state:
 * a character it can spell goes into the two-byte set, whose lead and trail
 * both range over 0x21..0x7E -- `,` and every other punctuation mark included
 * -- so a cell can forge any separator that is not ASCII. An ASCII one cannot
 * be forged, because the encoder shifts BACK to ASCII before writing it, and
 * the escape byte that shift begins with is below every pair byte.
 *
 * So each character comes back as one of exactly two shapes, and the sweep
 * asserts it is one of them rather than searching for a separator:
 *
 *   ESC $ B <lead> <trail> ESC ( B 'A'      a cell
 *   '?' 'A'                                 a character php cannot spell
 *
 * U+00A5 and U+203E are left out of the batch on purpose: they are the only
 * two characters that put the encoder into the ROMAN set, which it does not
 * leave for an ASCII separator, and their spelling is asserted on its own
 * below. */
const SEP = 'A';
/*
 * And the sweep is CHUNKED, because php's encoder has a length artifact this
 * table must not inherit: past about 63 KB of output it drops the shift back
 * out of the two-byte set every few thousand characters, and where it drops it
 * moves with the OUTPUT offset rather than with the character -- U+5FA9 loses
 * its shift in one range and keeps it in another that starts elsewhere. A
 * chunk short enough never to reach the artifact answers the same for every
 * character, and the parser below refuses the artifact's shape rather than
 * reading through it, so a chunk that grew into one would fail here.
 */
const CHUNK = 512;
function enc(string $utf8): string {
	$d = Dom\HTMLDocument::createEmpty(LABEL);
	return substr($d->saveHtml($d->createComment($utf8)),4,-3);
}
$cpl = [];
for( $cp = 0x80 ; $cp <= 0xFFFF ; $cp++ ){
	if( $cp >= 0xD800 && $cp <= 0xDFFF ) continue;
	if( $cp === 0x00A5 || $cp === 0x203E ) continue;
	$cpl[] = $cp;
}
$eCp = []; $eB = [];
foreach( array_chunk($cpl,CHUNK) as $aBatch ){
	$txt = '';
	foreach( $aBatch as $cp ){ $txt .= mb_chr($cp,'UTF-8').SEP; }
	$g = enc($txt); $at = 0;
	foreach( $aBatch as $cp ){
		if( substr($g,$at,1) === '?' ){
			$at += 1;                               /* php cannot spell it */
		}elseif( substr($g,$at,3) === ESC_JIS && substr($g,$at + 5,3) === RESET ){
			$eCp[] = $cp;
			$eB[] = (ord($g[$at + 3]) << 8) | ord($g[$at + 4]);
			$at += 8;
		}else{
			die_(sprintf('U+%04X encoded to %s',$cp,bin2hex(substr($g,$at,10))));
		}
		if( substr($g,$at,1) !== SEP ){
			die_(sprintf('U+%04X ran past its separator',$cp));
		}
		$at += 1;
	}
	if( $at !== strlen($g) ){ die_('a batch left '.(strlen($g) - $at).' bytes over'); }
}
/*
 * The rest of the encoder is LAW rather than table, and each row below is one
 * assertion about a law vm_dom.c spells in C. They are read off the whole
 * comment, terminator included, because the encoder's state runs through that
 * too: a document that ends in the Roman set writes `-->` inside it and shifts
 * back only at the very end of the stream.
 *
 *   A5/203E     the Roman set's 0x5C and 0x7E -- the ONLY two characters that
 *               enter it, and it is not left for an ordinary ASCII character.
 *   A5 then 5C  those two bytes are the two the Roman set redefines, so the
 *               characters themselves need the ASCII set back.
 *   0E/0F/1B    refused with `?` -- but only when the encoder is ALREADY in
 *               the ASCII or Roman set. Reached from the two-byte set the
 *               shift happens first and the byte is then written RAW, which is
 *               php's order of business and not the standard's.
 *   FFFD/astral no spelling at all.
 */
$law = [
	"\u{00A5}"          => ESC_ROMAN."\x5C".'-->'.RESET,
	"\u{203E}"          => ESC_ROMAN."\x7E".'-->'.RESET,
	"a\u{00A5}b"        => 'a'.ESC_ROMAN."\x5C".'b-->'.RESET,
	"\u{00A5}\x5C"      => ESC_ROMAN."\x5C".RESET."\x5C".'-->',
	"\u{00A5}\x7E"      => ESC_ROMAN."\x5C".RESET."\x7E".'-->',
	"\u{00A5}\u{001B}"  => ESC_ROMAN."\x5C".'?-->'.RESET,
	"a\u{001B}b"        => 'a?b-->',
	"a\u{000E}b"        => 'a?b-->',
	"a\u{000F}b"        => 'a?b-->',
	"\u{3000}\u{001B}z" => ESC_JIS."\x21\x21".RESET."\x1Bz-->",
	"\u{3000}a"         => ESC_JIS."\x21\x21".RESET.'a-->',
	"\u{FFFD}"          => '?-->',
	"\u{1F600}"         => '?-->',
	"\u{20000}"         => '?-->',
];
foreach( $law as $in => $out ){
	$d = Dom\HTMLDocument::createEmpty(LABEL);
	$g = substr($d->saveHtml($d->createComment($in)),4);
	if( $g !== $out ){
		die_(bin2hex($in).' encoded to '.bin2hex($g).', not '.bin2hex($out));
	}
}
fwrite(STDERR,sprintf("%s %d escapes, %d pairs, %d encoded\n",LABEL,count($esc),$n,count($eCp)));
/* --- emit --------------------------------------------------------------- */
$w = function(array $a){
	$o = ''; $a = array_values($a); $m = count($a);
	for( $i = 0 ; $i < $m ; $i++ ){
		$o .= ($i % 12 === 0 ? "\t" : '').sprintf('0x%04X',$a[$i]).($i + 1 === $m ? '' : ',');
		if( $i % 12 === 11 || $i + 1 === $m ) $o .= "\n";
	}
	return $o;
};
echo <<<'TXT'
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The HTML document's ISO-2022-JP index. GENERATED -- do not hand-edit;
 * re-cut with `XDEBUG_MODE=off php build-aux/gen_domenc_iso2022jp.php`.
 *
 * This is NOT iconv's or mbstring's ISO-2022-JP, and the generator holds the
 * sweep that says so. Only the two-byte set is a table: the three single-byte
 * sets, the escapes that select them and the encoder's shift discipline are
 * LAWS the generator measures byte by byte and fails on, and vm_dom.c spells
 * them in C rather than storing them.
 *
 *   aDomJisCell     the two-byte set, row (lead - 0x21), column (trail - 0x21).
 *                   0 is a cell php answers U+FFFD for. The lead and trail
 *                   windows are both 0x21..0x7E, swept, not assumed.
 *   aDomJisEncCp    the ENCODER, ordered by code point for a binary search,
 *   aDomJisEncB     with the lead<<8|trail it writes into the two-byte set.
 *                   U+00A5 and U+203E are not here: they are the Roman set's
 *                   0x5C and 0x7E, which is a law, not a cell.
 *
 * php recognises FIVE escapes and no escape longer than two bytes, so this
 * encoding has no JIS X 0212 set -- the one place it is narrower than the
 * EUC-JP that shares its repertoire.
 */

TXT;
printf("#define PH7_DOM_JIS_ENC %d\n",count($eCp));
printf("static const sxu16 aDomJisCell[%d * %d] = {\n%s};\n",NROW,NCOL,$w($cell));
printf("static const sxu16 aDomJisEncCp[PH7_DOM_JIS_ENC] = {\n%s};\n",$w($eCp));
printf("static const sxu16 aDomJisEncB[PH7_DOM_JIS_ENC] = {\n%s};\n",$w($eB));
