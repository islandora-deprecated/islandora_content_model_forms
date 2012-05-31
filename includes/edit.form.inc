<?php
/**
 * @file
 * Edit form callback and related functions.
 */

/**
 * Edit form callback.
 * 
 * @param $form
 * @param $form_state
 * @param $pid
 * @param $ds_id
 */
function islandora_content_model_forms_get_edit_form($form, &$form_state, $pid, $ds_id = NULL) {
  $page = $form_state['storage']['step'] = empty($form_state['storage']['step']) ? 1 : $form_state['storage']['step'];
  if ($page == 1) {
    $form_state['storage']['xml'] = TRUE;
    $form = islandora_content_model_forms_edit_page_1($pid, $ds_id, $form_state);
  }
  
  // If the form is set while fetching step 1, proceed to step 2 (case: one form).
  if (isset($form_state['values']['forms'])) {
    $page = 2;
    $form_state['storage']['step'] = 2;
  }
  
  if ($page == 2 && $form_state['storage']['xml']) {
    $form = islandora_content_model_forms_edit_page_2($pid, $form_state);
  }
    
  return $form;
}

/**
 * Datastream edit form step 1.
 * 
 * @param $pid
 * @param $ds_id
 * @param $form_state
 * @throws Exception
 */
function islandora_content_model_forms_edit_page_1($pid, $ds_id, &$form_state) {
  global $user;
  module_load_include('inc', 'islandora', 'RestConnection');
  
  $rest_connection = new RestConnection($user);
  $fedora_object = new FedoraObject($pid, $rest_connection->repository);
  $content_models = $fedora_object->models; //First??
  if (empty($ds_id)) {
    $ds_id = NULL;
  }
  $form_names = array();
  foreach ($content_models AS $model) {
    $form_names += islandora_content_model_get_form_names($model, $ds_id);
  }
  
  if (empty($form_names)) {
    throw new Exception('Couldn\'t Create Page One of the Edit Form.');
  }
  
  $form = array();
 
  if (count($form_names) > 1) {
    $form['indicator'] = array(
      '#type' => 'fieldset',
      '#title' => t('Choose edit form.'),
    );
    $form['indicator']['forms'] = array(
      '#type' => 'select',
      '#title' => t('Forms'),
      '#options' => $form_names,
      '#description' => t('Select the form to edit.'),
    );
    $form['submit'] = array(
      '#type' => 'submit',
      '#validate' => array('islandora_content_model_forms_get_edit_form_validate'),
      '#value' => t('Next'),
    );
  }
  else {
    $form_state['values']['forms'] = key($form_names);
  }
  
  return $form;
}

/**
 * Datastream edit form step 2.
 * 
 * @param $pid
 * @param $form_state
 */
function islandora_content_model_forms_edit_page_2($pid, &$form_state) {
  global $user;
  
  $form_name = $form_state['values']['forms'];
  list($dsid, $content_model) = islandora_content_model_forms_get_ids_for_form($form_name);
  
  module_load_include('inc', 'islandora', 'RestConnection');
  $rest_connection = new RestConnection($user);
  $fedora_object = new FedoraObject($pid, $rest_connection->repository);
  
  $xml = $fedora_object[$dsid];
  
  $form = xml_form_builder_get_form($form_state, $form_name, $xml->content);

  $form['content_model_pid'] = array(
    '#type' => 'hidden',
    '#value' => $content_model,
  );
  $form['forms'] = array(
    '#type' => 'hidden',
    '#value' => $form_state['values']['forms'],
  );
  $form['pid'] = array(
    '#type' => 'hidden',
    '#value' => $pid,
  );
  $form['dsid'] = array(
    '#type' => 'hidden',
    '#value' => isset($form_state['values']['dsid']) ? $form_state['values']['dsid'] : $dsid,
  );
  
  $form['#submit'] = array('islandora_content_model_forms_edit_form_submit');
  $form['#validate'] = array('islandora_content_model_forms_get_edit_form_validate');
  
  return $form;
}

/**
 * Edit form validation.
 * 
 * @param $form
 * @param $form_state
 */
function islandora_content_model_forms_get_edit_form_validate($form, &$form_state) {
  if ($form_state['storage']['xml']) {
    if ($form_state['storage']['step'] == 1) {
      $form_state['storage']['step']++;
      $form_state['rebuild'] = TRUE;
    }
  }
}

/**
 * Edit form submit.
 * 
 * @param $form
 * @param $form_state
 * @throws Exception
 */
function islandora_content_model_forms_edit_form_submit($form, &$form_state) {
  module_load_include('inc', 'xml_form_api', 'XMLForm');
  module_load_include('inc', 'islandora_content_model_forms', 'includes/ingest.form');
  global $base_url, $user;
  
  $pid = $form_state['values']['pid'];
  $dsid = $form_state['values']['dsid'];
  $form_name = $form_state['values']['forms'];
  $content_model_pid = $form_state['values']['content_model_pid'];
  $label_field = islandora_content_model_get_title_field($content_model_pid, $form_name);
  eval('$label = $form_state[\'values\']' . $label_field . ';');
  
  $xml_form = new XMLForm($form_state);
  $doc = $xml_form->submit($form, $form_state);
  $document = $doc->document;
  
  module_load_include('inc', 'islandora', 'RestConnection');
  $rest_connection = new RestConnection($user);
  $object = new FedoraObject($pid, $rest_connection->repository);
  $object->label = $label;
  
  $xml_ds = new FedoraDatastream($dsid, $object, $rest_connection->repository);
  $xml_ds->setContentFromString($document->saveXML());
  
  $transform = islandora_content_model_get_transform($content_model_pid, $form_name);
  $dc_doc = islandora_content_model_forms_transform_document($transform, $document, $object);
  $dc_ds = new FedoraDatastream('DC', $object, $rest_connection->repository);
  $dc_ds->setContentFromString($dc_doc->saveXML());
  
  $form_state['storage'] = NULL;
  $form_state['redirect'] = 'islandora/object/' . $pid;
}