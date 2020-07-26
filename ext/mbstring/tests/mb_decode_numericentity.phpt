--TEST--
Test mb_decode_numericentity() function : Convert HTML-Entities to UTF-8
--SKIPIF--
<?php
if (!extension_loaded('mbstring')) die('skip mbstring not enabled');
function_exists('mb_encode_mimeheader') or die("skip mb_encode_mimeheader() is not available in this build");
?>
--FILE--
<?php
$str1 = '&#161;&#162;&#163;&#164;&#165;&#166;&#167;&#168;&#169;&#170;&#171;&#172;&#173;&#174;&#175;&#176;&#177;&#178;&#179;&#180;&#181;&#182;&#183;&#184;&#185;&#186;&#187;&#188;&#189;&#190;&#191;&#192;&#193;&#194;&#195;&#196;&#197;&#198;&#199;&#200;&#201;&#202;&#203;&#204;&#205;&#206;&#207;&#208;&#209;&#210;&#211;&#212;&#213;&#214;&#215;&#216;&#217;&#218;&#219;&#220;&#221;&#222;&#223;&#224;&#225;&#226;&#227;&#228;&#229;&#230;&#231;&#232;&#233;&#234;&#235;&#236;&#237;&#238;&#239;&#240;&#241;&#242;&#243;&#244;&#245;&#246;&#247;&#248;&#249;&#250;&#251;&#252;&#253;&#254;&#255;';
$str2 = '&#402;&#913;&#914;&#915;&#916;&#917;&#918;&#919;&#920;&#921;&#922;&#923;&#924;&#925;&#926;&#927;&#928;&#929;&#931;&#932;&#933;&#934;&#935;&#936;&#937;&#945;&#946;&#947;&#948;&#949;&#950;&#951;&#952;&#953;&#954;&#955;&#956;&#957;&#958;&#959;&#960;&#961;&#962;&#963;&#964;&#965;&#966;&#967;&#968;&#969;&#977;&#978;&#982;&#8226;&#8230;&#8242;&#8243;&#8254;&#8260;&#8472;&#8465;&#8476;&#8482;&#8501;&#8592;&#8593;&#8594;&#8595;&#8596;&#8629;&#8656;&#8657;&#8658;&#8659;&#8660;&#8704;&#8706;&#8707;&#8709;&#8711;&#8712;&#8713;&#8715;&#8719;&#8721;&#8722;&#8727;&#8730;&#8733;&#8734;&#8736;&#8743;&#8744;&#8745;&#8746;&#8747;&#8756;&#8764;&#8773;&#8776;&#8800;&#8801;&#8804;&#8805;&#8834;&#8835;&#8836;&#8838;&#8839;&#8853;&#8855;&#8869;&#8901;&#8968;&#8969;&#8970;&#8971;&#9001;&#9002;&#9674;&#9824;&#9827;&#9829;&#9830;';
$str3 = 'a&#338;b&#339;c&#352;d&#353;e&#8364;fg';
$convmap = array(0x0, 0x2FFFF, 0, 0xFFFF);
echo mb_decode_numericentity($str1, $convmap, "UTF-8")."\n";
echo mb_decode_numericentity($str2, $convmap, "UTF-8")."\n";
echo mb_decode_numericentity($str3, $convmap, "UTF-8")."\n";

echo mb_decode_numericentity('&#1000000000', $convmap), "\n";
echo mb_decode_numericentity('&#9000000000', $convmap), "\n";
echo mb_decode_numericentity('&#10000000000', $convmap), "\n";
echo mb_decode_numericentity('&#100000000000', $convmap), "\n";

echo mb_decode_numericentity('&#000000000000', $convmap), "\n";
echo mb_decode_numericentity('&#00000000000', $convmap), "\n";
echo mb_decode_numericentity('&#0000000000', $convmap), "\n";
echo mb_decode_numericentity('&#000000000', $convmap), "\n";

$convmap = [];
echo mb_decode_numericentity('f&ouml;o', $convmap, "UTF-8")."\n";

// Try numeric entities which follow one after other without terminating semicolons
// Web browsers do accept and decode HTML numeric entities like this!
$convmap = array(0x0, 0x2FFFF, 0, 0xFFFF);
echo mb_decode_numericentity('&#65;&#66;', $convmap), "\n";
echo mb_decode_numericentity('&#x41;&#66;', $convmap), "\n";
echo mb_decode_numericentity('&#65;&#x42;', $convmap), "\n";
echo mb_decode_numericentity('&#x41;&#x42;', $convmap), "\n";

$convmap = array(0x0, 0x2FFFF, 0); // 3 elements
try {
    echo mb_decode_numericentity($str3, $convmap, "UTF-8")."\n";
} catch (ValueError $ex) {
    echo $ex->getMessage()."\n";
}

?>
--EXPECT--
¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ
ƒΑΒΓΔΕΖΗΘΙΚΛΜΝΞΟΠΡΣΤΥΦΧΨΩαβγδεζηθικλμνξοπρςστυφχψωϑϒϖ•…′″‾⁄℘ℑℜ™ℵ←↑→↓↔↵⇐⇑⇒⇓⇔∀∂∃∅∇∈∉∋∏∑−∗√∝∞∠∧∨∩∪∫∴∼≅≈≠≡≤≥⊂⊃⊄⊆⊇⊕⊗⊥⋅⌈⌉⌊⌋〈〉◊♠♣♥♦
aŒbœcŠdše€fg
&#1000000000
&#9000000000
&#10000000000
&#100000000000
&#000000000000
&#00000000000
&#0000000000
&#000000000
f&ouml;o
AB
AB
AB
AB
mb_decode_numericentity(): Argument #2 ($map) must have a multiple of 4 elements
