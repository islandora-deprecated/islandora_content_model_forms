<?php
/**
 * @file
 *   Holds the API reference material for the islandora_content_model_forms
 *   module.
 */

/**
 * Sets the POLICY to NULL.
 *
 * @param DOMDocumentFragment $policy_element
 *   The XML document fragment holding the XACML
 * @param string $PID
 *   The Fedora PID of the object.
 * @param string $collection_ID
 *   The Fedora PID of the collection the object belongs to.
 * @param string $content_model_PID
 *   The PID of the content model of the object.
 */
function hook_islandora_content_model_forms_policy_alter(&$policy_element, $PID, $collection_ID, $content_model_PID) {
  $policy_element = NULL;
}
