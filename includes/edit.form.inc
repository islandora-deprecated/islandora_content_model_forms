<?php

function islandora_content_model_forms_get_edit_form($form, &$form_state, $pid) {
  $page = $form_state['storage']['step'] = empty($form_state['storage']['step']) ? 1 : $form_state['storage']['step'];
    if ($page == 1) {
      $form_state['storage']['xml'] = TRUE;
      $form = islandora_content_model_forms_edit_page_1($pid, $form_state);
    }
    else if ($form_state['storage']['xml']) {
      $form = islandora_content_model_forms_edit_page_2($pid, $form_state);
    }
    else {
      throw new Exception('Couldn\'t Create the Edit Form.');
    }
    
    return $form;
}

function islandora_content_model_forms_get_edit_form_validate($form, &$form_state) {
  if ($form_state['storage']['xml']) {
    if ($form_state['storage']['step'] == 1) {
      $form_state['storage']['step']++;
      $form_state['rebuild'] = TRUE;
    }
  }
}

function islandora_content_model_forms_edit_form_submit($form, &$form_state) {
  module_load_include('inc', 'xml_form_api', 'XMLForm');
  module_load_include('inc', 'islandora_content_model_forms', 'includes/ingest.form');
  global $base_url;
    $pid = $form_state['values']['pid'];
    $dsid = $form_state['values']['dsid'];
    $form_name = $form_state['values']['forms'];
    $content_model_pid = $form_state['values']['content_model_pid'];
    $label_field = islandora_content_model_get_title_field($content_model_pid, $form_name);
    eval('$label = $form_state[\'values\']' . $label_field . ';');
    
    $xml_form = new XMLForm($form_state);
    $doc = $xml_form->submit($form, $form_state);
    $document = $doc->document;
    
    global $user;
    module_load_include('inc', 'islandora', 'RestConnection');
    $restConnection = new RestConnection($user);
    $object = new FedoraObject($pid, $restConnection->repository);
    
    $xml_ds = new FedoraDatastream($dsid, $object, $restConnection->repository);
    $xml_ds->setContentFromString($document->saveXML());
    
    $transform = islandora_content_model_get_transform($content_model_pid, $form_name);
    
    $dc_doc = islandora_content_model_forms_transform_document($transform, $document, $object);
    
    $dc_ds = new FedoraDatastream('DC', $object, $restConnection->repository);
    $dc_ds->setContentFromString($dc_doc->saveXML());
    
    $form_state['storage'] = NULL;
    $form_state['redirect'] = 'islandora/object/' . $pid;
}

function islandora_content_model_forms_edit_page_1($pid, &$form_state) {
  global $user;
    module_load_include('inc', 'islandora', 'RestConnection');
    $restConnection = new RestConnection($user);
    $fedora_object = new FedoraObject($pid, $restConnection->repository);
    $content_model = $fedora_object->models[0]; //First??
  
  $form_names = islandora_content_model_get_form_names($content_model);
    if (empty($form_names)) {
      throw new Exception('Couldn\'t Create Page One of the Edit Form.');
    }
    
    $form = array();
    
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
        '#value' => 'Next',
    );
    
    $form['content_model_pid'] = array(
      '#type' => 'hidden',
      '#value' => $content_model,
    );
    
    return $form;
}

function islandora_content_model_forms_edit_page_2($pid, &$form_state) {
    $form_name = $form_state['values']['forms'];
    $dsid = islandora_content_model_get_dsid($form_state['values']['content_model_pid'], $form_name);
    global $user;
     module_load_include('inc', 'islandora', 'RestConnection');
    $restConnection = new RestConnection($user);
    $fedora_object = new FedoraObject($pid, $restConnection->repository);
    $xml = $fedora_object[$dsid];
    
    $form = xml_form_builder_get_form($form_state, $form_name, $xml->content);
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