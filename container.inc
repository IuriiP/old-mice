<?php

/**
 * Core class for dynamic container organisation.
 *
 * <pre>
 * <plugin name="container" id="unique_id">
 * [content for each linked plugin]
 * </plugin>
 * </pre>
 */
class container
        extends \Controller {
    function __invoke( $args = null ) {
        return($this);  // on
    }
    function process( $el ) {
        $fragment = $this->moveTo($el, $el->ownerDocument->createDocumentFragment());
        if( ($id = $el->getAttribute('id')) && isset($this->config[$id]) ) {
            foreach( $this->config[$id] as $pname => $value ) {
                if( $value ) {
                    $plugin = $el->ownerDocument->createElement('plugin');
                    $plugin->setAttribute('name', $pname);
                    $el->appendChild($plugin);
                    $plugin->appendChild($fragment->cloneNode(true));
                }
            }
        }
        return(parent::process($el));
    }
}

?>