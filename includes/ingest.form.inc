<?php
/**
 * @file
 * Ingest form callback and related functions.
 */

/**
 * Multi-step ingest form callback.
 * Step 1: determine desired XML form.
 * Step 2: render XML form.
 *
 * @param $form
 * @param $form_state
 * @param $collection_pid
 * @throws Exception
 */
function islandora_content_model_forms_get_ingest_form($form, &$form_state, $collection_pid) {
  module_load_include('module', 'islandora', 'islandora');
  $page = $form_state['storage']['step'] = empty($form_state['storage']['step']) ? 1 : $form_state['storage']['step'];
  $collection_object = islandora_object_load($collection_pid);
  $collection_label = $collection_object->label;

  if ($page == 1) {
    $form_state['storage']['xml'] = TRUE;
    $form_state['storage']['collection_pid'] = $collection_pid;
    $form = islandora_content_model_forms_ingest_page_1($form_state, $collection_pid, $collection_object->label);
  }

  // If the form is set while fetching step 1, proceed to step 2 (case: one form).
  if (isset($form_state['values']['forms'])) {
    $page = 2;
    $form_state['storage']['step'] = 2;
  }

  if ($page == 2) {
    $form = islandora_content_model_forms_ingest_page_2($form_state, $collection_pid, $collection_object->label);
  }

  return $form;
}

/**
 * Ingest form step 1.
 *
 * @param $form_state
 * @param $collection_pid
 * @param $collection_label
 */
function islandora_content_model_forms_ingest_page_1(&$form_state, $collection_pid, $collection_label) {
  $object = islandora_object_load($collection_pid);
  drupal_set_title(t('Ingest'));

  $content_models = islandora_content_model_forms_get_potential_content_models($object);
  $form_names = array();
  $form_count = 0;
  foreach ($content_models AS $model => $name) {
    $forms = islandora_content_model_get_form_names($model);
    foreach($forms as $form_name => $friendly_name) {
      $form_names[$name]["$model $form_name"] = $friendly_name;
      $form_count++;
    }
  }

  if (count($content_models) == 0) {
    drupal_set_message(t('No content models are associated with this collection. Please add a content model and try again.'), 'warning');
    drupal_goto("islandora/object/$collection_pid");
  }

  if ($form_count == 0) {
    drupal_set_message(t('No forms are associated. Please associate a form and try again.'), 'warning');
    drupal_goto("islandora/object/$collection_pid");
  }

  $form = array();

  if($form_count > 1) {
    $form['indicator'] = array(
      '#type' => 'fieldset',
      '#title' => t('Ingest digital object into %collection_label (@collection_pid), Step #1:', array('@collection_pid' => $collection_pid, '%collection_label' => $collection_label)),
    );

    $form['indicator']['forms'] = array(
      '#type' => 'select',
      '#title' => t('Metadata Forms Available'),
      '#options' => $form_names,
      '#description' => t("Choose a content model and metadata form, then click the 'Next' button to continue the ingest process."),
    );

    $form['collection_pid'] = array(
      '#type' => 'hidden',
      '#value' => $collection_pid,
    );

    $form['submit'] = array(
      '#type' => 'submit',
      '#validate' => array('islandora_content_model_forms_get_ingest_form_validate'),
      '#value' => t('Next'),
    );    
  }
  else {
    $model = key($form_names);
    $form_state['values']['forms'] = key($form_names[$model]);
    $form_state['values']['collection_pid'] = $collection_pid;
  }

  return $form;
}

/**
 * Ingest form step 2.
 *
 * @param $form_state
 * @param $collection_pid
 * @param $collection_label
 */
function islandora_content_model_forms_ingest_page_2(&$form_state, $collection_pid, $collection_label) {
  drupal_set_title(t('Ingest form'));
  
  list($content_model, $form_name) = explode(' ', $form_state['values']['forms'], 2);
  $dsid = islandora_content_model_get_dsid($content_model, $form_name);

  $xml = NULL;
  if (!empty($form_state['storage']['xml_data'])) {
    $xml = $form_state['storage']['xml_data'];
  }

  $form = xml_form_builder_get_form($form_state, $form_name, $xml);
  $form['collection_pid'] = array(
    '#type' => 'hidden',
    '#value' => $collection_pid,
  );
  $form['content_model'] = array(
    '#type' => 'hidden',
    '#value' => $content_model,
  );
  $form['dsid'] = array(
    '#type' => 'hidden',
    '#value' => $dsid,
  );
  $form['form_name'] = array(
    '#type' => 'hidden',
    '#value' => $form_name,
  );

  $form['#submit'] = array('islandora_content_model_forms_ingest_form_submit');
  $form['#validate'] = array('islandora_content_model_forms_get_ingest_form_validate');
  return $form;
}

/**
 * Ingest form validation.
 *
 * @param $form
 * @param $form_state
 */
function islandora_content_model_forms_get_ingest_form_validate($form, &$form_state) {
  // Only validate the form if the submit button was pressed (other buttons may be used for AJAX)
  if (!$form_state['submitted']) {
    return;
  }

  switch ($form_state['storage']['step']) {
    case 1:
      $form_state['storage']['step']++;
      $form_state['rebuild'] = TRUE;
      break;

    case 2:
      // XML based form.
      if ($form_state['storage']['xml']) {
        module_load_include('inc', 'xml_form_api', 'XMLForm');
        $xml_form = new XMLForm($form_state);
        $xml_form->validate($form, $form_state);
      }

      // Handle uploaded files.
      $arr = array();
      /*
      if (!empty($types_allowed)) {
        foreach ($types_allowed as $type) {
          $arr[] = $mimetype->getExtension($type);
        }
      }
      */

      if (!empty($_FILES['files']['name']['ingest-file-location'])) {
        $file_object = file_save_upload('ingest-file-location', array('file_validate_extensions' => $arr));

        if ($file_object) {
          $form_state['values']['ingest-file-location'] = $file_object->uri;
          $form_state['values']['ingest-file-location-id'] = $file_object->fid;
        }
      }
      $form_state['rebuild'] = FALSE;
      break;
  }
}

/**
 * Ingest form submit.
 *
 * @param $form
 * @param $form_state
 */
function islandora_content_model_forms_ingest_form_submit($form, &$form_state) {
  // Only submit the form if the submit button was pressed (other buttons may be used for AJAX)
  if (!$form_state['submitted']) {
    return;
  }

  module_load_include('inc', 'xml_form_api', 'XMLForm');
  module_load_include('inc', 'islandora', 'includes/islandora.ingest');
  global $base_url, $user;

  if (isset($_FILES['files']['name']['file_upload']) && $_FILES['files']['type']['file_upload'] == 'text/xml') {
    $tmp_dir = file_directory_path();
    if (!$file = file_save_upload('file_upload', array('file_validate_extensions' => array()), $tmp_dir)) {
      drupal_set_message(t('No XML file loaded'), 'warning');
      return;
    }
    $file = "$tmp_dir/{$file->filename}";
    $xml_file = file_get_contents($file);
    $document = DOMDocument::loadXml($xml_file);
    unlink($file);
    if (!$document) {
      drupal_set_message(t('This does not appear to be a valid XML file'), 'warning');
      return;
    }
  }
  else {
    $xml_form = new XMLForm($form_state);
    $doc = $xml_form->submit($form, $form_state);
    $document = $doc->document;
  }

  $collection_pid = $form_state['values']['collection_pid'];
  $content_model_pid = $form_state['values']['content_model'];
  $dsid = $form_state['values']['dsid'];
  $form_name = $form_state['values']['form_name'];
  $should_redirect = TRUE;

  $relationship = NULL;
  $pid = NULL;

  $fedora_object = islandora_object_load($collection_pid);
  $collection_policy_info = islandora_ingest_get_information($fedora_object);

  $relationship = $collection_policy_info['relationship'];

  $content_models = array(array('pid' => $content_model_pid));

  $object = islandora_ingest_get_object($content_models, $collection_pid, $collection_policy_info['relationship'], 'islandora');
  $transform = islandora_content_model_get_transform($content_model_pid, $form_name);
  $label_field = islandora_content_model_get_title_field($content_model_pid, $form_name);
  eval('$label = $form_state[\'values\']' . $label_field . ';');

  $object->label = drupal_substr($label, 0, 256);
  $object->owner = $user->name;

  if (!isset($form['#post']['active-toggle'])) {
    $state = 'A';
  }
  else {
    $state = $form['#post']['active-toggle'] ? 'A' : 'I';
  }
  $object->state = $state;

  $ds = $object->constructDatastream($dsid, 'X');
  $ds->setContentFromString($document->saveXML());
  $ds->label = $dsid . ' Record';
  $object->ingestDatastream($ds);

  if ($dsid != 'DC' && $transform) {
    $dc_ds = $object->constructDatastream('DC', 'X');
    $dc_ds->label = 'DC Record';
    $object->ingestDatastream($dc_ds);
    $dc_doc = islandora_content_model_forms_transform_document($transform, $document, $object);
    $dc_ds->setContentFromString($dc_doc->saveXML());
  }

  if (!empty($relationship) && !empty($object->id)) {

    $uploaded_file = isset($form_state['values']['ingest-file-location']) && trim($form_state['values']['ingest-file-location']) !== '';
    $ingest_file_location = $uploaded_file ? $form_state['values']['ingest-file-location'] : NULL;
    if (isset($ingest_file_location)) {
      $ingest_file_location = is_array($ingest_file_location) ? $ingest_file_location : array('OBJ' => $ingest_file_location);
    }

    islandora_content_model_forms_ingest_files($ingest_file_location, $object);

    $form_state['storage'] = NULL;
  }

  $form_state['redirect'] = "islandora/object/{$collection_pid}";
  $object = islandora_ingest_add_object($object);

  if (isset($form_state['values']['ingest-file-location-id'])) {
    file_delete(file_load($form_state['values']['ingest-file-location-id']));
  }
}

/**
 * Get content models associated with a collection.
 *
 * @param $collection_pid
 * @param $form_state
 */
function islandora_content_model_forms_get_potential_content_models($collection_object) {
  module_load_include('inc', 'islandora', 'includes/islandora.ingest');
  $info = islandora_ingest_get_information($collection_object);

  $potential_models = array();
  if (isset($info['models']) && is_array($info['models'])) {
    foreach ($info['models'] AS $model) {
      $cm = islandora_object_load($model['pid']);
      $potential_models[$model['pid']] = $cm ? $cm->label : $model['name'];
    }
  }

  return $potential_models;
}

/**
 * Transform form generated XML.
 *
 * @param $transform
 * @param $source_document
 * @param $object
 */
function islandora_content_model_forms_transform_document($transform, $source_document, $object) {
  if ($transform !== NULL) {
    $xsl = new DOMDocument();
    $xsl->load($transform);
    $xslt = new XSLTProcessor();
    $xslt->importStyleSheet($xsl);
    $document = $xslt->transformToDoc($source_document);
    if ($document) {
      $xpath = new DOMXPath($document);
      // Set the Label
      $results = $xpath->query("*[local-name()='title']");
      $results->item(0)->nodeValue = $object->label;
      // Set the Pid
      $results = $xpath->query("*[local-name()='identifier']");
      $results->item(0)->nodeValue = $object->id;
      if (isset($document->documentElement)) {
        return $document;
      }
    }
  }
  return NULL;
}

/**
 * Add a managed datastream to an object based on an uploaded file.
 *
 * @param $ingest_file_locations
 * @param $object
 */
function islandora_content_model_forms_ingest_files($ingest_file_locations, $object) {
  if (empty($ingest_file_locations) || count($ingest_file_locations) === 0) {
    return;
  }
  foreach ($ingest_file_locations as $dsid => $file) {
    list($label, $mime_type) = _islandora_content_model_forms_get_file_attributes($file);

    $ds = $object->constructDatastream($dsid, 'M');
    $file = drupal_realpath($file);
    $ds->setContentFromFile($file);
    $ds->label = $label;
    $ds->mimetype = $mime_type;
    $object->ingestDatastream($ds);
  }
}

/**
 * Determine a title and mimetype for a file.
 *
 * @param $file
 */
function _islandora_content_model_forms_get_file_attributes($file) {
  module_load_include('inc', 'islandora', 'includes/mime.detect');
  global $base_url;

  $mime = new MimeDetect();
  $mime_type = $mime->getMimetype($file);
  $parts = explode('/', $file);
  foreach ($parts as $n => $part) {
    $parts[$n] = rawurlencode($part);
  }
  $path = implode('/', $parts);
  $file_url = $base_url . '/' . $path;
  $begin_index = strrpos($file_url, '/');
  $dtitle = drupal_substr($file_url, $begin_index + 1);
  $dtitle = urldecode($dtitle);
  return array($dtitle, $mime_type);
}
