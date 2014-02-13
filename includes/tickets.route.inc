<?php
function pos_api_expose_ticket_statuses() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not load ticket stasuses.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			$ticket_stasuses = array();
			$sql = "SELECT order_status_id, title FROM {uc_order_statuses}";
			$order_stasuses = db_query($sql);
			while($order_stasus = db_fetch_object($order_stasuses)) {
				if(strpos($order_stasus->title, 'Ecom ::') === FALSE) { //Take away ecommerce ticket stasuses.
					$ticket_stasuses[$order_stasus->order_status_id] = $order_stasus->title;
				}
			}

			pos_api_expose_respond(array('status' => true, 'stasuses' => $ticket_stasuses), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_new_ticket() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not create a new ticket.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			//Initial ticket is always for guest customers for now. Be set active user on ticket id change to
			//ignore any active customer selections done while the ticket is being created in the db.

			$order = new stdClass();
			$order->uid = 0;
			$order->order_status = 'pos_in_progress';
			$order->order_total = 0;
			$order->product_count = 0;
			$order->delivery_zone = 0;
			$order->delivery_country = 840;
			$order->billing_zone = 0;
			$order->billing_country = 840;
			$order->data = array();
			$order->created = time();
			$order->modified = time();
			$order->host = ip_address();
			$order->currency = 'USD';

			drupal_write_record('uc_orders', $order);
			pos_api_expose_respond(array('status' => true,
										 'ticketId' => $order->order_id,
										 'ticketStatus' =>$order->order_status), true);
		}
	}

	pos_api_expose_respond($failed_response);
}