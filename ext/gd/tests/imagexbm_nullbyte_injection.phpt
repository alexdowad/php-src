--TEST--
Testing null byte injection in imagexbm
--SKIPIF--
<?php
if(!extension_loaded('gd')) die('skip gd extension not available');
?>
--FILE--
<?php
$image = imagecreate(1,1);// 1px image
try {
    imagexbm($image, "./foo\0bar");
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECTF--
imagexbm(): Argument #2 ($filename) must be a valid path or null, string given
