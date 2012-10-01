<?php

/**
 * @file
 * This file lists and documents all available hook functions to manipulate
 * data.
 */

/**
 * This function allows the modules to programmatically define form
 * associations. These assoications can be disabled, but cannot be removed
 * without disabling the module that creates them
 *
 * @return array
 *   This function should return an array of the form:
 *   @code
 *   return array(
 *     'unique_assoication_name' => array( # by convention this should start with your modules name
 *     'content_model' => 'islandora:sp_basic_image',
 *     'form_name' => 'Image DC Form',
 *     'dsid' => 'DC',
 *     'title_field' => "['dc:title']",
 *     'transform' => 'No Transform',
 *     'template' => FALSE,
 *   );
 *   @endcode
 */
function hook_islandora_content_model_forms_form_associations() {}

/**
 * This function allows modules to programmatically define new transforms
 * that can be applied when a form is submitted.
 *
 * @return array
 *   This function should return an arary of the form:
 *   @code
 *   return array(
 *     'unique transform name' => 'full/path/to/transform.xsl',
 *   );
 *   @endcode
 */
function hook_islandora_content_model_forms_get_transforms() {}