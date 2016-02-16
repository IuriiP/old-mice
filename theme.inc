<?php

/**
 * Core class for using themed request.
 *
 * @author Iurii Prudius <hardwork.mouse@gmail.com>
 */
class theme
        extends \Controller {
  private $attrs = null;

  /**
   * @param array $args Controller attributes
   */
  function __construct( $args = null ) {
    $this->attrs = (array) $args;
    if( !$this->theme ) {
      $this->set($this->attrs['default']);
    } else {
      $this->set($this->theme);
    }
  }
  /**
   * Store existed theme name into cookie for next using.
   * 
   * @param string $theme Theme name
   */
  public function set( $theme ) {
    if( $this->makePath($this->rootdir, 'model/'.$theme) ) {
      setcookie('theme', $theme, PHP_INT_MAX, '/');
    }
  }
  /**
   * Conventional function to forget stored settings.
   *
   * @return bool true (for chaining)
   */
  public function forget() {
    setcookie('theme');
  }
  /**
   * Invoking.
   * 
   * @return $this
   */
  public function __invoke( $args = null ) {
    return($this);
  }
  
  public function process( $node ) {
    if( $path=$this->findModel($this->type.'.xml') ) {
      self::includeXML( file_get_contents($path), $node );
      return( parent::process($node) );
    }
    return($node);
  }
}

?>
