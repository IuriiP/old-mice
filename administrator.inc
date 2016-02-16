<?php

require_once('constants.inc');
require_once('Controller.inc');
require_once('Helper.inc');

/**
 * Administrator level
 *
 * <pre>
 * plugin name="administrator"
 * </pre>
 *
 */
class administrator
        extends \Controller {
    private $attrs = null;

    /**
     *  Invoking
     *
     *  @return $this | NULL
     */
    function __invoke( $args = null ) {
        $this->attrs = $args;
        if( $this->security->aid == DB_USER_ACCESS_MASTER ) {
            return($this);
        }
        return(null);
    }
}

?>