<?php

/**
 * Interface plugin to find default main page
 * 
 * Use config settings to include 
 */
class main
        extends \Controller {
  /**
   * @param array $args
   * @return Controller
   */
  function __invoke( $args = null ) {
    return($this);
  }
  function event_main( $node ) {
    if( ($page = $this->config['page']) && ($page!='main') ) {
      echo "<plugin name=\"{$page}\"><event /></plugin>";
      return($page);
    } else {
      if( $model = $this->view() ) {
        echo $model->ownerDocument->saveXML($model);
        return(true);
      }
    }
    return(false);
  }
}

?>