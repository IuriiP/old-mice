<?php

require_once('Helper.inc');

class lipsum
        extends Controller {
  private $atts = null;
  /**
   * Internal 'Lorem Ipsum' generator.
   * @var LoremIpsumGenerator
   */
  private static $generator = null;

  function __construct() {
    if( !self::$generator ) {
      self::$generator = new LoremIpsumGenerator(file(__DIR__."/{$this->context}.txt",FILE_IGNORE_NEW_LINES + FILE_SKIP_EMPTY_LINES));
    }
  }
  public function __invoke( $args = null ) {
    $this->atts = $args;
    if( self::$generator ) {
      return($this);
    }
    return(null);
  }
  /**
   * Replace content of the plugin by generated fake text.
   * 
   * @param DOMElement $node
   */
  function process( $node ) {
    self::removeChildren($node);
    $attr = Helper::attr2arr($node);
    if( self::$generator
            && ($content = self::$generator->getContent(Helper::getVar($attr, 'count'), Helper::getVar($attr, 'format', $this->type)) ) ) {
      self::includeXML($content, $node);
    }
    return($node);
  }
}

class LoremIpsumGenerator {
  private $words;

  function __construct( $words ) {
    $this->words = array_fill_keys($words,true);
  }
  function getContent( $count, $format = 'html', $loremipsum = true ) {
    if( $count <= 0 ) {
      $count = rand(2, 8);
    }
    switch( $format ) {
      case 'xml':
      case 'json':
        return "<{$format}>".$this->getXML($count)."</{$format}>";
      case 'html':
        return "<{$format}>".$this->getHTML($count)."</{$format}>";
      default:
        return "<{$format}>".$this->getPlain($count)."</{$format}>";
    }
  }
  private function getPlain( $count) {
    $arr = $this->getParagraphs($count);
    return implode(PHP_EOL,$arr);
  }
  private function getXML( $count) {
    $arr = $this->getParagraphs($count);
    foreach($arr as $key=>$para) {
      $tag = strtolower(array_rand($this->words));
      $arr[$key] = "<{$tag}>{$para}</{$tag}>";
    }
    return implode(PHP_EOL,$arr);
  }
  private function getHTML( $count ) {
    $arr = $this->getParagraphs($count);
    foreach($arr as $key=>$para) {
      $arr[$key] = "<p>{$para}</p>";
    }
    return implode(PHP_EOL,$arr);
  }
  private function getParagraphs( $count ) {
    $arr = array();
    $loremipsum = true;
    for($i=0;$i<$count;$i++) {
      $arr[] = implode('. ', $this->getParagraph(rand(2,10),$loremipsum)).'.';
      $loremipsum = false;
    }
    return($arr);
  }
  private function getParagraph( $sentences_count, $loremipsum ) {
    $arr = array();
    for($i=0;$i<$sentences_count;$i++) {
      $sentence = array_rand($this->words, rand(5,18));
      if( $loremipsum ) {
        array_unshift($sentence, 'Lorem ipsum');
      }
      $loremipsum = false;
      if( count($sentence) > 12 ) {
        $sentence[rand(6, count($sentence) - 5)] .= array_rand(array( ',' => 0, ':' => 0, ' -' => 0 ));
      }
      $arr[] = ucfirst(implode(' ',$sentence));
    }
    return($arr);
  }
}
