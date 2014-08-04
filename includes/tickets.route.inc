<?php
define("INVOICE_VOCABULARY", 1);

function _pos_api_expose_customer_link($uid) {
	if($uid) {
		$user = db_fetch_object(db_query("SELECT u.name, n.title FROM {users} u LEFT JOIN {node} n ON u.uid = n.uid WHERE u.uid = '%s'", $uid));
		if($user->title) {
			$uname = $user->title." (".$user->name.")";
		} else {
			$uname = $user->name;
		}

		return '<a class="ticket-customer-link" href="#" data-uid="'.$uid.'">'.$uname.'</a>';
    } else {
    	return 'Walk-In';
    }
}

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
				if(strpos($order_stasus->title, 'Ecom ::') === FALSE) {
					//Take away ecommerce ticket stasuses.
					$ticket_stasuses[$order_stasus->order_status_id] = $order_stasus->title;
				}
			}

			$categories = array();
			$invoice_terms = db_query("SELECT tid, name FROM {term_data} WHERE vid = '%d'", INVOICE_VOCABULARY);

			while($term = db_fetch_object($invoice_terms)) {
				$categories[$term->name] = 0;
			}

			pos_api_expose_respond(array('status' => true, 'stasuses' => $ticket_stasuses, 'categories' => $categories), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_ticket_totals() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not load ticket stasuses.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$uid = _pos_api_expose_uid_from_token($request->token);
		$ticketId = $request->ticketId;
		if($uid) {
			pos_api_expose_ticket_update_total_on_server($ticketId);
			$total = db_result(db_query("SELECT order_total FROM uc_orders WHERE order_id = '%d'", $ticketId));
			$taxes = db_result(db_query("SELECT SUM(amount) FROM uc_order_line_items WHERE order_id = '%d' AND type = 'tax'", $ticketId));

			pos_api_expose_respond(array('status' => true, 'total' => $total, 'taxes' => $taxes), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function _pos_api_expose_employee_order_history($uid) {
  $header = array(
  	array('data' => t('Last Opened')),
    array('data' => t('Order #')),
    array('data' => t('Customer')),
    array('data' => t('Status')),
    array('data' => t('Total')),
    array('data' => t('Created On'))
  );

  $rows = array();

  $context = array(
    'revision' => 'themed-original',
    'type' => 'amount',
  );
  $list = uc_order_status_list('general', TRUE);
  $result = pager_query("SELECT uc.order_id, uc.uid, uc.created, uc.order_total AS total, log.timestamp, os.title FROM (SELECT * FROM {pos_api_expose_ticket_log} tl GROUP BY ticket_id ORDER BY MAX(timestamp) DESC) log INNER JOIN uc_orders uc ON log.ticket_id = uc.order_id INNER JOIN uc_order_statuses os ON uc.order_status = os.order_status_id WHERE log.uid = '%d'", 20, 0, "SELECT COUNT(*) FROM (SELECT * FROM {pos_api_expose_ticket_log} tl GROUP BY ticket_id ORDER BY MAX(timestamp) DESC) t2 WHERE t2.uid = '%d'", $uid);

  // Build a table based on the customer's orders.
  while ($order = db_fetch_object($result)) {
    $context['subject'] = array('order' => $order);

    $link = $order->order_id;

    if (user_access('view all orders')) {
      $link .= '<span class="order-admin-icons">'. uc_order_actions($order, TRUE) .'</span>';
    }

    $rows[] = array(
    	array('data' => format_date($order->timestamp, 'custom', variable_get('uc_date_format_default', 'm/d/Y'))),
      array('data' => $link),
      array('data' => _pos_api_expose_customer_link($order->uid)),
      array('data' => check_plain($order->title)),
      array('data' => uc_price($order->total, $context), 'align' => 'right'),
      array('data' => format_date($order->created, 'custom', variable_get('uc_date_format_default', 'm/d/Y')))
    );
  }

  if (empty($rows)) {
    $rows[] = array(array('data' => t('No orders available.'), 'colspan' => 6));
  }

  $content = theme('table', $header, $rows, array('class' => 'pure-table uc-order-history'))
       . theme('pager', NULL, 20, 0);

  return str_replace('src="/misc/', 'src="images/misc/', $content);
}

function _pos_api_expose_quote_order_history() {
  $header = array(
    array('data' => t('Date'), 'field' => 'o.created', 'sort' => 'desc'),
    array('data' => t('Order #'), 'field' => 'o.order_id'),
    array('data' => t('Customer')),
    array('data' => t('Status'), 'field' => 'os.title'),
    array('data' => t('Products'), 'field' => 'products'),
    array('data' => t('Total'), 'field' => 'o.order_total')
  );

  $rows = array();

  $context = array(
    'revision' => 'themed-original',
    'type' => 'amount',
  );

  $result = pager_query("SELECT o.order_id, o.uid, o.created, os.title, SUM(op.qty) AS products, o.order_total AS total, o.order_status FROM {uc_orders} AS o LEFT JOIN {uc_order_statuses} AS os ON o.order_status = os.order_status_id LEFT JOIN {uc_order_products} AS op ON o.order_id = op.order_id WHERE o.order_status IN ('pos_return', 'pos_return_closed') GROUP BY o.order_id, o.uid, o.created, os.title, o.order_total, o.order_status". tablesort_sql($header), 20, 0, "SELECT COUNT(*) FROM {uc_orders} o WHERE o.order_status IN ('pos_return', 'pos_return_closed')");

  // Build a table based on the customer's orders.
  while ($order = db_fetch_object($result)) {
    $context['subject'] = array('order' => $order);

    $link = $order->order_id;

    if (user_access('view all orders')) {
      $link .= '<span class="order-admin-icons">'. uc_order_actions($order, TRUE) .'</span>';
    }

    $rows[] = array(
      array('data' => format_date($order->created, 'custom', variable_get('uc_date_format_default', 'm/d/Y'))),
      array('data' => $link),
      array('data' => _pos_api_expose_customer_link($order->uid)),
      array('data' => check_plain($order->title)),
      array('data' => (!is_null($order->products) ? $order->products : 0), 'align' => 'center'),
      array('data' => uc_price($order->total, $context), 'align' => 'right'),
    );
  }

  if (empty($rows)) {
    $rows[] = array(array('data' => t('No orders available.'), 'colspan' => 6));
  }

  $content = theme('table', $header, $rows, array('class' => 'pure-table uc-order-history'))
       . theme('pager', NULL, 20, 0);

  return str_replace('src="/misc/', 'src="images/misc/', $content);
}

function _pos_api_expose_open_order_history($cuid = 0) {
  if($cuid) {
	  $header = array(
	    array('data' => t('Date')),
	    array('data' => t('Order #')),
	    array('data' => t('Products')),
	    array('data' => t('Total'))
	  );
  } else {
  	$header = array(
	    array('data' => t('Date'), 'field' => 'o.created', 'sort' => 'desc'),
	    array('data' => t('Order #'), 'field' => 'o.order_id'),
	    array('data' => t('Customer')),
	    array('data' => t('Status'), 'field' => 'os.title'),
	    array('data' => t('Products'), 'field' => 'products'),
	    array('data' => t('Total'), 'field' => 'o.order_total')
	  );
  }

  $rows = array();

  $context = array(
    'revision' => 'themed-original',
    'type' => 'amount',
  );
  if($cuid) {
  	$result = pager_query("SELECT o.order_id, o.uid, o.created, os.title, SUM(op.qty) AS products, o.order_total AS total, o.order_status FROM {uc_orders} AS o LEFT JOIN {uc_order_statuses} AS os ON o.order_status = os.order_status_id LEFT JOIN {uc_order_products} AS op ON o.order_id = op.order_id WHERE o.order_status = 'pos_in_progress' AND o.uid = '".db_escape_string($cuid)."' GROUP BY o.order_id, o.uid, o.created, os.title, o.order_total, o.order_status". tablesort_sql($header), 20, 0, "SELECT COUNT(*) FROM {uc_orders} o WHERE o.order_status = 'pos_in_progress' AND o.uid = '".db_escape_string($cuid)."'");
  } else {
  	$result = pager_query("SELECT o.order_id, o.uid, o.created, os.title, SUM(op.qty) AS products, o.order_total AS total, o.order_status FROM {uc_orders} AS o LEFT JOIN {uc_order_statuses} AS os ON o.order_status = os.order_status_id LEFT JOIN {uc_order_products} AS op ON o.order_id = op.order_id WHERE o.order_status = 'pos_in_progress' GROUP BY o.order_id, o.uid, o.created, os.title, o.order_total, o.order_status". tablesort_sql($header), 20, 0, "SELECT COUNT(*) FROM {uc_orders} o WHERE o.order_status = 'pos_in_progress'");
  }

  // Build a table based on the customer's orders.
  while ($order = db_fetch_object($result)) {
    $context['subject'] = array('order' => $order);

    $link = $order->order_id;

    /*if (user_access('view all orders')) {
      $link .= '<span class="order-admin-icons">'. uc_order_actions($order, TRUE) .'</span>';
    }*/

    if($cuid) {
	    $rows[] = array(
	      array('data' => format_date($order->created, 'custom', variable_get('uc_date_format_default', 'm/d/Y'))),
	      array('data' => $link),
	      array('data' => (!is_null($order->products) ? $order->products : 0), 'align' => 'center'),
	      array('data' => uc_price($order->total, $context), 'align' => 'right'),
	    );
	} else {
		$rows[] = array(
	      array('data' => format_date($order->created, 'custom', variable_get('uc_date_format_default', 'm/d/Y'))),
	      array('data' => $link),
	      array('data' => _pos_api_expose_customer_link($order->uid)),
	      array('data' => check_plain($order->title)),
	      array('data' => (!is_null($order->products) ? $order->products : 0), 'align' => 'center'),
	      array('data' => uc_price($order->total, $context), 'align' => 'right'),
	    );
	}
  }

  if (empty($rows)) {
  	if($cuid) {
    	$rows[] = array(array('data' => t('No orders available.'), 'colspan' => 6));
	} else {
		$rows[] = array(array('data' => t('No orders available.'), 'colspan' => 4));
	}
  }

  $content = theme('table', $header, $rows, array('class' => 'pure-table uc-order-history'))
       . theme('pager', NULL, 20, 0);

  return str_replace('src="/misc/', 'src="images/misc/', $content);
}

function _pos_api_expose_closed_order_history() {
  $header = array(
    array('data' => t('Date'), 'field' => 'o.created', 'sort' => 'desc'),
    array('data' => t('Order #'), 'field' => 'o.order_id'),
    array('data' => t('Customer')),
    array('data' => t('Status'), 'field' => 'os.title'),
    array('data' => t('Products'), 'field' => 'products'),
    array('data' => t('Total'), 'field' => 'o.order_total')
  );

  $rows = array();

  $context = array(
    'revision' => 'themed-original',
    'type' => 'amount',
  );

  $result = pager_query("SELECT o.order_id, o.uid, o.created, os.title, SUM(op.qty) AS products, o.order_total AS total, o.order_status FROM {uc_orders} AS o LEFT JOIN {uc_order_statuses} AS os ON o.order_status = os.order_status_id LEFT JOIN {uc_order_products} AS op ON o.order_id = op.order_id WHERE o.order_status = 'pos_completed' GROUP BY o.order_id, o.uid, o.created, os.title, o.order_total, o.order_status". tablesort_sql($header), 20, 0, "SELECT COUNT(*) FROM {uc_orders} o WHERE o.order_status = 'pos_completed'");

  // Build a table based on the customer's orders.
  while ($order = db_fetch_object($result)) {
    $context['subject'] = array('order' => $order);

    $link = $order->order_id;

    if (user_access('view all orders')) {
      $link .= '<span class="order-admin-icons">'. uc_order_actions($order, TRUE) .'</span>';
    }

    $rows[] = array(
      array('data' => format_date($order->created, 'custom', variable_get('uc_date_format_default', 'm/d/Y'))),
      array('data' => $link),
      array('data' => _pos_api_expose_customer_link($order->uid)),
      array('data' => check_plain($order->title)),
      array('data' => (!is_null($order->products) ? $order->products : 0), 'align' => 'center'),
      array('data' => uc_price($order->total, $context), 'align' => 'right'),
    );
  }

  if (empty($rows)) {
    $rows[] = array(array('data' => t('No orders available.'), 'colspan' => 6));
  }

  $content = theme('table', $header, $rows, array('class' => 'pure-table uc-order-history'))
       . theme('pager', NULL, 20, 0);

  return str_replace('src="/misc/', 'src="images/misc/', $content);
}

function pos_api_expose_closed_invoices() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not get quote employee.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$uid = _pos_api_expose_uid_from_token($request->token);

		if($uid) {
			$invoices = _pos_api_expose_closed_order_history();
			pos_api_expose_respond(array('status' => true, 'content' => $invoices), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_open_invoices($cuid = 0) {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not get quote employee.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$uid = _pos_api_expose_uid_from_token($request->token);

		if($uid) {
			$invoices = _pos_api_expose_open_order_history($cuid);
			pos_api_expose_respond(array('status' => true, 'content' => $invoices), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_quote_invoices() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not get quote employee.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$uid = _pos_api_expose_uid_from_token($request->token);

		if($uid) {
			$invoices = _pos_api_expose_quote_order_history();
			pos_api_expose_respond(array('status' => true, 'content' => $invoices), true);
		}
	}

	pos_api_expose_respond($failed_response);
}


function pos_api_expose_recent_invoices() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not get recent employee invoices.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$uid = _pos_api_expose_uid_from_token($request->token);

		if($uid) {
			$invoices = _pos_api_expose_employee_order_history($uid);
			pos_api_expose_respond(array('status' => true, 'content' => $invoices), true);
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
			$order->order_status = 'pos_quote'; //Open New ticket as a quote ticket
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
										 'ticketStatus' =>$order->order_status,
										 'customerUid' => $order->uid), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_ticket_update_customer() {
	//Todo: update prices on database based on user role using data from special price module.
	$failed_response = array(
		'status' => false,
		'error' => 'Could not update a ticket.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$ticketId = $request->ticketId;
		$customerUid = $request->customerUid;

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			if($ticketId) {
				db_query("UPDATE {uc_orders} SET uid = '%d' WHERE order_id = '%d'", $customerUid, $ticketId);
				pos_api_expose_ticket_update_total_on_server($ticketId);
			
				if($customerUid) {
					//Get the minumum role price later like client, right now getting last.
					$rids = db_query("SELECT ur.rid FROM {users_roles} ur WHERE ur.uid = '%d'", $customerUid);

					while($role = db_fetch_object($rids)) {
						$rid = $role->rid;
						$nodes = db_query("SELECT uop.nid, upprp.price FROM {uc_order_products} uop 
							INNER JOIN {uc_price_per_role_prices} upprp on uop.nid = upprp.nid
							WHERE uop.order_id = '%d' AND upprp.rid = '%d'", $ticketId, $rid);
						while($node = db_fetch_object($nodes)) {
							if($node->price) {
								db_query("UPDATE {uc_order_products} SET price = '%f' WHERE order_id = '%d' AND nid = '%d'", $node->price, $ticketId, $node->nid);
							}
						}
					}
				} else {
					//Reset to sell price
					$nodes = db_query("SELECT uop.nid, up.sell_price FROM {uc_order_products} uop
							 INNER JOIN {uc_products} up ON uop.nid = up.nid
							 WHERE uop.order_id = '%d'", $ticketId);

					while($node = db_fetch_object($nodes)) {
						db_query("UPDATE {uc_order_products} SET price = '%f' WHERE order_id = '%d' AND nid = '%d'", $node->sell_price, $ticketId, $node->nid);
					}
				}
			}

			pos_api_expose_respond(array('status' => true, 'message' => 'Customer id has been set.'), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_ticket_update_product_qty() {
		$request = json_decode($_POST['request']);
                $ticketId = $request->ticketId;
                $qtyGraph = $request->qtyGraph;
		$queries = '';
                $uid = _pos_api_expose_uid_from_token($request->token);
                if($uid) {
                        if($ticketId) {
                                foreach($qtyGraph as $qtyUpdate) { 
					db_query("UPDATE {uc_order_products} SET qty = '%d' WHERE order_id = '%d' AND order_product_id = '%d'", $qtyUpdate->qty, $ticketId, $qtyUpdate->productId);
				}
                        }

                        pos_api_expose_respond(array('status' => true, 'message' => 'Product qty updated. Test'.$queries), true);
                }
}

function pos_api_expose_ticket_current() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not get ticket current customer.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$ticketId = $request->ticketId;

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			$sql = "INSERT INTO {pos_api_expose_ticket_log} (uid, ticket_id, timestamp) VALUES ('%d', '%d', '%d')";
			db_query($sql, $uid, $ticketId, time());

			$sql = "SELECT uo.order_id, uo.uid, uo.order_status, os.title AS order_status_en
					FROM uc_orders uo
					INNER JOIN uc_order_statuses os ON uo.order_status = os.order_status_id
					WHERE uo.order_id = '%d'";
			$item = db_fetch_object(db_query($sql, $ticketId));
			pos_api_expose_respond(array('status' => true,
				'ticket' => array('ticketId' => $item->order_id,
										 'status' => $item->order_status,
										 'status_en' => $item->order_status_en,
										 'customerUid' => $item->uid)), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_ticket_search($token, $query) {
	if(!empty($token)) {
		$uid = _pos_api_expose_uid_from_token($token);

		if($uid) {
			//By Search Company Name with Fluid Revelevance
			$fulltext_sql = "SELECT nv.vid, MATCH(nv.title) AGAINST ('%s') AS relevance
							 FROM {node_revisions} nv
							 HAVING relevance > 0";

			$company_name = "SELECT nv.vid, 15 AS relevance
							 FROM node_revisions nv
							 WHERE UPPER(nv.title) LIKE UPPER('%%%s%%')";

			//Search by Account ID
			$accountid_search = "SELECT n.vid, 10 AS relevance
								 FROM node n
								 INNER JOIN users u ON n.uid = u.uid
								 WHERE u.name LIKE '%s%%'";

			//Search by Phone Number
			$phone_searches = "SELECT ctp.vid, 9 AS relevance
							   FROM content_type_profile ctp
							   WHERE ctp.field_profile_company_phone_number LIKE '%s%%' OR
							   ctp.field_profile_phone_number LIKE '%s%%'";

			//Get all possible customers form search query.
			$ticket_customers = "SELECT u.uid
					FROM node n
					INNER JOIN users u ON n.uid = u.uid
					INNER JOIN ((".$fulltext_sql.") UNION (".$accountid_search.") UNION (".$phone_searches.") UNION (".$company_name."))
					search ON search.vid = n.vid
					WHERE n.type = 'profile'";

			$sql = "SELECT order_id, uid, order_status, os.title AS order_status_en
					FROM uc_orders uo
					INNER JOIN uc_order_statuses os ON uo.order_status = os.order_status_id
					WHERE uo.uid IN (".$ticket_customers.") OR uo.order_id LIKE '%s%%'
					ORDER BY uo.modified DESC";

			$dataset = array();
			$items = db_query($sql, $query, $query, $query, $query, $query, $query);

			while($item = db_fetch_object($items)) {
				$company_name = db_result(db_query("SELECT title FROM {node} WHERE type ='profile' AND uid = '%d'", $item->uid));
				$dataset[] = array('ticketId' => $item->order_id,
										 'companyName' => $company_name,
										 'ticketStatus' => $item->order_status,
										 'ticketStatus_en' => $item->order_status_en,
										 'customerUid' => $item->uid);
			}

			pos_api_expose_respond($dataset, true);
		}
	}

	pos_api_expose_respond(array());
}

function pos_api_expose_ticket_add_product() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not add product to ticket.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);

		$uid = _pos_api_expose_uid_from_token($request->token);

		if($uid) {
			$cuid = $request->customerUid;
			$order_product = new stdClass();
			$order_product->order_id = $request->ticketId;
			$order_product->nid = $request->productId;
			$order_product->title = $request->name;
			$order_product->model = $request->sku;

			if($cuid > 0) {
				$price = _pos_api_get_customer_price($cuid, $order_product->nid);
				if($price) {
					$order_product->price = $price;
				} else {
					$order_product->price = $request->price;
				}
			} else {
				$order_product->price = $request->price;
			}


			$order_product->cost = pos_stock_management_average_cost($order_product->nid);
			if(empty($order_product->cost)) {
				//Use default automagic cost.
				$order_product->cost = db_result(db_query("SELECT cost FROM {uc_products} WHERE nid = '%d'", $order_product->nid));
			}

			$order_product->manufacturer = '';
			$order_product->weight = 0;
			$order_product->qty = 1;
			$order_product->data = array(
				'attributes' => array()
			);

			drupal_write_record('uc_order_products', $order_product);

			pos_api_expose_respond(array('status' => true,
										 'ticketProductId' => $order_product->order_product_id), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function _pos_api_get_customer_price($customer_uid, $nid) {
	return db_result(db_query("SELECT up.price FROM users_roles ur
	 INNER JOIN uc_price_per_role_prices up ON ur.rid = up.rid
	 WHERE ur.uid = '%d' AND up.nid = '%d'", $customer_uid, $nid));
}

function pos_api_expose_ticket_remove_product() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not remove product from ticket.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$ticketId = $request->ticketId;
		$productId = $request->productId;

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			db_query("DELETE FROM {uc_order_products} WHERE order_id = '%d' AND order_product_id = '%d'", $ticketId, $productId);
			db_query("DELETE FROM {pos_api_expose_manager_override_log} WHERE ticket_id = '%d' AND product_nid = '%d'", $ticketId, $productId);
			pos_api_expose_respond(array('status' => true,
										 'message' => 'Ticket product removed.'), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_ticket_update_total_on_server($ticketId) {
	//Trigger Tax calculation and Total update on server via hooks.
	$order = uc_order_load($ticketId);
	//Save manager overrides
	$overrides = db_query("SELECT ticket_id, product_nid, unit_price FROM {pos_api_expose_manager_override_log} ol WHERE ol.ticket_id = '%d'", $ticketId);
	while($override = db_fetch_object($overrides)) {
		db_query("UPDATE {uc_order_products} SET price = '%s' WHERE order_id = '%d' AND nid = '%d'", $override->unit_price, $override->ticket_id, $override->product_nid);
	}
	//Create Tax Line Item
	uc_order_module_invoke('save', $order, NULL);
	//Refresh to calculate current Price
	uc_order_load($ticketId);
}

function pos_api_expose_get_order_total($ticketId) {
	pos_api_expose_ticket_update_total_on_server($ticketId);
	return db_result(db_query("SELECT order_total FROM uc_orders WHERE order_id = '%d'", $ticketId));
}

function pos_api_expose_ticket_update_totals() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not update totals on ticket.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$ticketId = $request->ticketId;
		//$total = $request->total;
		$product_count = $request->productCount;

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			pos_api_expose_ticket_update_total_on_server($ticketId);
			pos_api_expose_respond(array('status' => true,
										 'message' => 'Ticket totals updated.'), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_process_rma() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not update rma refund, please check network and try again.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$ticketId = $request->ticketId;
		$customer_uid = $request->customer_uid;
		$register_id = $request->register_id;
		$credit_return = $request->credit_return;

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			if($ticketId) {
				$return_logs = db_query("SELECT pid FROM {pos_api_expose_rma_refund_log} WHERE rma_ticket_id = '%d'", $ticketId);
				while($return_log = db_fetch_object($return_logs)) {
					db_query("DELETE FROM {pos_api_expose_transaction_log} WHERE rma_pid = '%d'", $return_log->pid);
				}

				//db_query("DELETE FROM {user_term_credits_usages} WHERE order_id = '%d'", $ticketId);
				db_query("DELETE FROM {pos_api_expose_rma_refund_log} WHERE rma_ticket_id = '%d'", $ticketId);
				db_query("DELETE FROM {pos_api_expose_rma_returned_products} WHERE rma_ticket_id = '%d'", $ticketId);

				//Delete current RMA logs
				$rma_products = db_query("SELECT nid, qty, price FROM uc_order_products WHERE order_id = '%d'", $ticketId);
				$amount_returned = 0;
				while($rma_product = db_fetch_object($rma_products)) {
					$amount_returned += $rma_product->price*$rma_product->qty;
					$purchased_items = db_query("SELECT uop.order_product_id, uo.created, uop.order_id, uop.nid, uop.title, uop.model, uop.price, uop.qty, ctp.field_prod_packing_value, f.filepath, (uop.qty-IFNULL(rma.qty_returned,0)) AS remaining_qty
						FROM {uc_order_products} uop
						INNER JOIN {uc_orders} uo ON uop.order_id = uo.order_id
						LEFT JOIN {content_type_product} ctp ON uop.nid = ctp.nid
						LEFT JOIN {content_field_image_cache} cfic ON uop.nid = cfic.nid
						LEFT JOIN {files} f ON f.fid = cfic.field_image_cache_fid
						LEFT JOIN (SELECT order_product_id, SUM(qty_returned) AS qty_returned FROM {pos_api_expose_rma_returned_products} GROUP BY order_product_id) rma ON uop.order_product_id = rma.order_product_id
						WHERE uo.uid = '%d' AND uop.nid = '%d' AND uo.order_status = 'pos_completed' AND (uop.qty > rma.qty_returned OR ISNULL(rma.qty_returned))", $customer_uid, $rma_product->nid);

					while($purchased_item = db_fetch_object($purchased_items)) {
						if($rma_product->qty > 0) {
							if($rma_product->qty <= $purchased_item->remaining_qty) {
								_pos_api_expose_create_return_entry($rma_product->nid, $purchased_item->order_product_id, $rma_product->qty, $rma_product->price, $ticketId);
								$rma_product->qty = 0;
							} elseif($purchased_item->remaining_qty > 0 && $rma_product->qty > $purchased_item->remaining_qty) {
								_pos_api_expose_create_return_entry($rma_product->nid, $purchased_item->order_product_id, $purchased_item->remaining_qty, $rma_product->price, $ticketId);
								$rma_product->qty = $rma_product->qty-$purchased_item->remaining_qty;
							}
						} else {
							break;
						}
					}

					if($rma_product->qty > 0) {
						_pos_api_expose_create_return_entry($rma_product->nid, 0, $rma_product->qty, $rma_product->price, $ticketId);
					}
				}

				//Stop sending ticket to credit usages all together.
				/*db_query("INSERT INTO {user_term_credits_usages} (uid, order_id, credit_amount, term_limit, consumption_date, due_date, settled) VALUES ('%d', '%d', '-%s', '0', '%s', '%s', '1')", $customer_uid, $ticketId, $amount_returned, time(), time());*/

				//$credit_usage_id = $rma_pid = db_last_insert_id('user_term_credits_usages', 'cuid');
				
				//Credit usage id will be used as rma used flag.
				if($credit_return) {
					db_query("INSERT INTO {pos_api_expose_rma_refund_log} (uid, cuid, credit_usage_id, total_refund, rma_ticket_id, `timestamp`)
					VALUES ('%d', '%d', '%d', '%s', '%d', '%s')", $uid, $customer_uid, 0, $amount_returned, $ticketId, time());
				
					$rma_pid = db_last_insert_id('pos_api_expose_rma_refund_log', 'pid');
					$message = 'RMA Credit was processed in the amount of $'.number_format($amount_returned, 2);
				} else {
					$rma_pid = -1;
					$message = 'RMA CASH was processed, please provide customer with $'.number_format($amount_returned, 2);
				}

				db_query("INSERT INTO {pos_api_expose_transaction_log} (uid, cuid, amount, rma_pid, ticket_id, register_id, `timestamp`) VALUES ('%d', '%d', '%s', '%d', '%d', '%s', '%s')", $uid, $customer_uid, $amount_returned, $rma_pid, $ticketId, $register_id, time());

				pos_api_expose_respond(array('status' => true,
											 'message' => $message), true);

			}
		}
	}

	pos_api_expose_respond($failed_response);
}


function _pos_api_expose_create_return_entry($nid, $order_product_id, $qty_returned, $price, $rma_ticket_id) {
	db_query("INSERT INTO {pos_api_expose_rma_returned_products} (order_product_id, qty_returned, refund_log_pid, price, rma_ticket_id) VALUES ('%d', '%d', '%d', '%s', '%d')", $order_product_id, $qty_returned, 0, $price, $rma_ticket_id);

	//Put RMA Ticket id here to be cleared.
	db_query("INSERT INTO {pos_out_stock_management} (product_nid, stock_adjustment, unit_price, `timestamp`) VALUES ('%d', '%d', '%s', '%s')", $nid, (-1)*$qty_returned, $price, time());
}

function pos_api_expose_update_rma_refund() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not update rma refund, please check network and try again.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$ticketId = $request->ticketId;
		$customer_uid = $request->customer_uid;

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			if($ticketId) {
				pos_api_expose_ticket_update_total_on_server($ticketId);
				$subtotal = db_result(db_query("SELECT (-1)*(uo.order_total-IFNULL(t.tax, 0)) AS subtotal FROM uc_orders uo
					LEFT JOIN (SELECT order_id, SUM(amount) AS tax FROM uc_order_line_items GROUP BY order_id) t
					ON uo.order_id = t.order_id WHERE uo.order_id = '%d'", $ticketId));

				if($subtotal < 0) {
					$time = time();

					db_query("DELETE FROM user_term_credits_usages WHERE order_id = '%d'", $ticketId);
					db_query("INSERT INTO user_term_credits_usages (uid, order_id, credit_amount, consumption_date, due_date)
						VALUES ('%d', '%d', '%s', '%s', '%s')", $customer_uid, $ticketId, $subtotal, $time, $time);

					pos_api_expose_respond(array('status' => true,
											 'message' => 'RMA ticket was re-processed.'), true);
				} else {
					pos_api_expose_respond(array('status' => true,
											 'message' => 'RMA tickets must amount to more than $0.00 to be re-processed.'), true);
				}
			}
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_ticket_update_zone() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not update delivery zone on ticket.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$ticketId = $request->ticketId;

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			$zone = db_result(db_query("SELECT billing_zone FROM uc_orders WHERE order_id = '%d'", $ticketId));

			if($zone) {
				$zone = 0;
			} else {
				$zone = 1;
			}

			db_query("UPDATE {uc_orders} SET billing_zone = '%d' WHERE order_id = '%d'", $zone, $ticketId);
			pos_api_expose_respond(array('status' => true,
										 'message' => 'Ticket delivery zone updated.'), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_ticket_swipe_checkout() {
	$failed_response = array(
		'status' => false,
		'error' => 'We could not read this card, please try again.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$zone = $request->zone;
		$uid = _pos_api_expose_uid_from_token($request->token);

		if($uid) {
			module_load_include('php', 'pos_api_expose', 'includes/anet_php_sdk/AuthorizeNet');

			if($zone == 1) {
				$cc_log_table = 'pos_api_expose_credit_card_log_dp';
				$transac_log_table = 'pos_api_expose_transaction_log_dp';
			} else {
				$cc_log_table = 'pos_api_expose_credit_card_log';
				$transac_log_table = 'pos_api_expose_transaction_log';
			}

			$test_mode = variable_get('pos_api_expose_testing_enabled', 1);
			if($test_mode) {
				$api_login = variable_get('pos_api_expose_login_id_test', '');
				$transac_key = variable_get('pos_api_expose_transac_key_test', '');
				define("AUTHORIZENET_SANDBOX", true);
			} else {
				$api_login = variable_get('pos_api_expose_login_id', '');
				$transac_key = variable_get('pos_api_expose_transac_key', '');
				define("AUTHORIZENET_SANDBOX", false);
			}

			if( !empty($transac_key) && !empty($api_login) ) {
				define("AUTHORIZENET_API_LOGIN_ID", $api_login);
				define("AUTHORIZENET_TRANSACTION_KEY", $transac_key);

				$ticketId = $request->ticketId;
				$cuid = $request->customer;
				$total = pos_api_expose_get_order_total($ticketId);
				$register_id = $request->register_id;

				$cardData = $request->cardData;

				if($cardData->hasTrack1 || $cardData->hasTrack2) {
					$sale = new AuthorizeNetAIM;
					$sale->amount = $total;
					$sale->card_num = $cardData->account;
					$sale->exp_date = $cardData->exp_month.'/'.$cardData->exp_year;

					if($cardData->hasTrack1) {
						$sale->first_name = $cardData->first_name;
						$sale->last_name = $cardData->last_name;
					}

					$response = $sale->authorizeAndCapture();

					if ($response->approved) {
						$sql = "INSERT INTO {".$cc_log_table."} (uid, cuid, ticket_id, amount_paid, transaction_id, `timestamp`) VALUES ('%d', '%d', '%d', '%s', '%s', %s)";
						db_query($sql, $uid, $cuid, $ticketId, $total, $response->transaction_id, time());
						$credit_pid = db_last_insert_id($cc_log_table, 'pid');

						$sql = "INSERT INTO {".$transac_log_table."} (uid, cuid, ticket_id, amount, cash_pid, cc_pid, check_pid, mo_pid, register_id, `timestamp`) VALUES ('%d', '%d', '%d', '%s', '%d', '%d', '%d', '%d', '%s', '%s')";
						db_query($sql, $uid, $cuid, $ticketId, $total, 0, $credit_pid, 0, 0, $register_id, time());

						_pos_api_expose_stock_adjustment($ticketId);

						pos_api_expose_respond(
							array("status" => true,
								  "message" => "Credit card payment successful.")
						, true);
					} else {
						pos_api_expose_respond(array(
							'status' => false,
							'error' => $response->response_reason_text
						), true);
					}
				}
			} else {
				pos_api_expose_respond(array(
					'status' => false,
					'error' => 'Credit card processing has not been configured.'
				), true);
			}
		} else {
			pos_api_expose_respond(array(
				'status' => false,
				'error' => 'You do not have permission to conduct this operation.'
			), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_ticket_partial_checkout() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not checkout ticket, please check network and try again.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$uid = _pos_api_expose_uid_from_token($request->token);
		$zone = $request->zone;

		if($uid) {
			module_load_include('php', 'pos_api_expose', 'includes/anet_php_sdk/AuthorizeNet');
			if($zone == 1) {
				$cash_log_table = 'pos_api_expose_cash_log_dp';
				$check_log_table = 'pos_api_expose_check_log_dp';
				$mo_log_table = 'pos_api_expose_money_order_log_dp';
				$cc_log_table = 'pos_api_expose_credit_card_log_dp';
				$transac_log_table = 'pos_api_expose_transaction_log_dp';
			} else {
				$cash_log_table = 'pos_api_expose_cash_log';
				$check_log_table = 'pos_api_expose_check_log';
				$mo_log_table = 'pos_api_expose_money_order_log';
				$cc_log_table = 'pos_api_expose_credit_card_log';
				$transac_log_table = 'pos_api_expose_transaction_log';
			}

			$test_mode = variable_get('pos_api_expose_testing_enabled', 1);
			if($test_mode) {
				$api_login = variable_get('pos_api_expose_login_id_test', '');
				$transac_key = variable_get('pos_api_expose_transac_key_test', '');
				define("AUTHORIZENET_SANDBOX", true);
			} else {
				$api_login = variable_get('pos_api_expose_login_id', '');
				$transac_key = variable_get('pos_api_expose_transac_key', '');
				define("AUTHORIZENET_SANDBOX", false);
			}

			if( !empty($transac_key) && !empty($api_login) ) {
				define("AUTHORIZENET_API_LOGIN_ID", $api_login);
				define("AUTHORIZENET_TRANSACTION_KEY", $transac_key);

				$ticketId = $request->ticketId;
				$total = pos_api_expose_get_order_total($ticketId);
				$cash = $request->cash;
				$change = $request->change;
				$cuid = $request->customer;

				$cash_value = $request->cash_val;

				$check = $request->check;
				$check_val = $request->check_val;
				$check_number = $request->check_number;
				$post_dated = $request->check_post_dated;
				$check_date = $request->check_date;

				$mo = $request->mo;
				$mo_val = $request->mo_val;
				$mo_ref = $request->mo_ref;

				$rma = $request->rma_credit_used;
				$rma_credit = $request->rma_credit;

				//$credit = $request->credit;
				//$credit_val = $request->credit_val;
				//$transac_id = $request->transac_id;
				$remaining = $request->remaining_balance;
				$register_id = $request->register_id;

				$cardData = $request->cardData;

				if($cardData->hasTrack1 || $cardData->hasTrack2) {
					$sale = new AuthorizeNetAIM;
					$sale->amount = $remaining;
					$sale->card_num = $cardData->account;
					$sale->exp_date = $cardData->exp_month.'/'.$cardData->exp_year;

					if($cardData->hasTrack1) {
						$sale->first_name = $cardData->first_name;
						$sale->last_name = $cardData->last_name;
					}

					$response = $sale->authorizeAndCapture();

					if ($response->approved) {
						$sql = "INSERT INTO {".$cc_log_table."} (uid, cuid, ticket_id, amount_paid, transaction_id, `timestamp`) VALUES ('%d', '%d', '%d', '%s', '%s', %s)";
						db_query($sql, $uid, $cuid, $ticketId, $remaining, $response->transaction_id, time());

						$credit_pid = db_last_insert_id($cc_log_table, 'pid');

						//Cash values
						$sql = "INSERT INTO {".$cash_log_table."} (uid, cuid, ticket_id, amount_paid, `change`, total, `timestamp`) VALUES ('%d', '%d', '%d', '%s', '%s', '%s', '%s')";
						//When change is sent to credits it will be 0 here.
						db_query($sql, $uid, $cuid, $ticketId, $cash_value, $change, $total, time());
						$cash_pid = db_last_insert_id($cash_log_table, 'pid');

						$check_pid = 0;
						if($check) {
							$sql = "INSERT INTO {".$check_log_table."} (uid, cuid, ticket_id, paid_amount, post_dated, cash_date, `timestamp`, check_number) VALUES ('%d', '%d', '%d', '%s', '%d', '%s', '%s', '%s')";
							db_query($sql, $uid, $cuid, $ticketId, $check_val, $post_dated, $check_date, time(), $check_number);
							$check_pid = db_last_insert_id($check_log_table, 'pid');
						}

						$mo_pid = 0;
						if($mo) {
							$sql = "INSERT INTO {".$mo_log_table."} (uid, cuid, ticket_id, paid_amount, reference, `timestamp`) VALUES ('%d', '%d', '%d', '%s', '%s', '%s')";
							db_query($sql, $uid, $cuid, $ticketId, $mo_val, $mo_ref, time());
							$mo_pid = db_last_insert_id($mo_log_table, 'pid');
						}

						$rma_pid = 0;
						if($rma) {
							$sql = "INSERT INTO {pos_api_expose_rma_refund_log} (uid, cuid, total_refund, `timestamp`) VALUES ('%d', '%d', '-%s', '%s')";
							db_query($sql, $uid, $cuid, $rma_credit, time());
							$rma_pid = db_last_insert_id('pos_api_expose_rma_refund_log', 'pid');
						}

						$sql = "INSERT INTO {".$transac_log_table."} (uid, cuid, ticket_id, amount, cash_pid, cc_pid, check_pid, mo_pid, register_id, `timestamp`, rma_pid)
							VALUES ('%d', '%d', '%d', '%s', '%d', '%d', '%d', '%d', '%s', '%s', '%d')";
						db_query($sql, $uid, $cuid, $ticketId, $total, $cash_pid, $credit_pid, $check_pid, $mo_pid, $register_id, time(), $rma_pid);

						_pos_api_expose_stock_adjustment($ticketId);

						pos_api_expose_respond(
							array("status" => true,
								  "message" => "Credit card payment successful.")
						, true);
					} else {
						pos_api_expose_respond(array(
							'status' => false,
							'error' => $response->response_reason_text
						), true);
					}
				} else {
					pos_api_expose_respond(array(
						'status' => false,
						'error' => 'We could not read this card, please try again.'
					), true);
				}
			} else {
				pos_api_expose_respond(array(
					'status' => false,
					'error' => 'Credit card processing has not been configured.'
				), true);
			}
		} else {
			pos_api_expose_respond(array(
				'status' => false,
				'error' => 'You do not have permission to conduct this operation.'
			), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_ticket_cash_checkout() {
	$failed_response = array(
		'status' => false,
		'message' => 'Could not checkout ticket, please check network and try again.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$ticketId = $request->ticketId;
		$total = pos_api_expose_get_order_total($ticketId);
		$cash = $request->cash;
		$change = $request->change;
		$cuid = $request->customer;

		$cash_value = $request->cash_val;

		$check = $request->check;
		$check_val = $request->check_val;
		$check_number = $request->check_number;
		$post_dated = $request->check_post_dated;
		$check_date = $request->check_date;

		$mo = $request->mo;
		$mo_val = $request->mo_val;
		$mo_ref = $request->mo_ref;

		$rma = $request->rma_credit_used;
		$rma_credit = $request->rma_credit;

		$credit = $request->credit;
		$credit_val = $request->credit_val;
		$transac_id = $request->transac_id;
		$register_id = $request->register_id;
		$zone = $request->zone;

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			if($zone == 1) {
				$cash_log_table = 'pos_api_expose_cash_log_dp';
				$check_log_table = 'pos_api_expose_check_log_dp';
				$mo_log_table = 'pos_api_expose_money_order_log_dp';
				$cc_log_table = 'pos_api_expose_credit_card_log_dp';
				$transac_log_table = 'pos_api_expose_transaction_log_dp';
			} else {
				$cash_log_table = 'pos_api_expose_cash_log';
				$check_log_table = 'pos_api_expose_check_log';
				$mo_log_table = 'pos_api_expose_money_order_log';
				$cc_log_table = 'pos_api_expose_credit_card_log';
				$transac_log_table = 'pos_api_expose_transaction_log';
			}

			$sql = "INSERT INTO {".$cash_log_table."} (uid, cuid, ticket_id, amount_paid, `change`, total, `timestamp`) VALUES ('%d', '%d', '%d', '%s', '%s', '%s', '%s')";
			//When change is sent to credits it will be 0 here.
			db_query($sql, $uid, $cuid, $ticketId, $cash_value, $change, $total, time());
			$cash_pid = db_last_insert_id($cash_log_table, 'pid');

			$check_pid = 0;
			if($check) {
				$sql = "INSERT INTO {".$check_log_table."} (uid, cuid, ticket_id, paid_amount, post_dated, cash_date, `timestamp`, check_number) VALUES ('%d', '%d', '%d', '%s', '%d', '%s', '%s', '%s')";
				db_query($sql, $uid, $cuid, $ticketId, $check_val, $post_dated, $check_date, time(), $check_number);
				$check_pid = db_last_insert_id($check_log_table, 'pid');
			}

			$mo_pid = 0;
			if($mo) {
				$sql = "INSERT INTO {".$mo_log_table."} (uid, cuid, ticket_id, paid_amount, reference, `timestamp`) VALUES ('%d', '%d', '%d', '%s', '%s', '%s')";
				db_query($sql, $uid, $cuid, $ticketId, $mo_val, $mo_ref, time());
				$mo_pid = db_last_insert_id($mo_log_table, 'pid');
			}

			$credit_pid = 0;
			if($credit) {
				$sql = "INSERT INTO {".$cc_log_table."} (uid, cuid, ticket_id, amount_paid, transaction_id, `timestamp`) VALUES ('%d', '%d', '%d', '%s', '%s', %s)";
				db_query($sql, $uid, $cuid, $ticketId, $credit_val, $transac_id, time());
				$credit_pid = db_last_insert_id($cc_log_table, 'pid');
			}

			$rma_pid = 0;
			if($rma) {
				$sql = "INSERT INTO {pos_api_expose_rma_refund_log} (uid, cuid, total_refund, `timestamp`) VALUES ('%d', '%d', '-%s', '%s')";
				db_query($sql, $uid, $cuid, $rma_credit, time());
				$rma_pid = db_last_insert_id('pos_api_expose_rma_refund_log', 'pid');
			}

			$sql = "INSERT INTO {".$transac_log_table."} (uid, cuid, ticket_id, amount, cash_pid, cc_pid, check_pid, mo_pid, register_id, `timestamp`, rma_pid)
				VALUES ('%d', '%d', '%d', '%s', '%d', '%d', '%d', '%d', '%s', '%s', '%d')";
			db_query($sql, $uid, $cuid, $ticketId, $total, $cash_pid, $credit_pid, $check_pid, $mo_pid, $register_id, time(), $rma_pid);

			_pos_api_expose_stock_adjustment($ticketId);

			pos_api_expose_respond(
				array("status" => true,
					  "message" => "Cash payment recorded.")
			, true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_credit_cash_checkout() {
	$failed_response = array(
		'status' => false,
		'message' => 'Could not complete payment, please check network and try again.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			$cash = $request->cash;
			$change = $request->change;
			$cuid = $request->customer;

			$cash_value = $request->cash_val;

			if($change > 0 && bccomp($change, '0.005', 3) == -1) {
				$change = 0;
				$cash = bcadd($change, $cash, 6);
				$cash_value = bcadd($change, $cash_value, 6);
			}

			$check = $request->check;
			$check_val = $request->check_val;
			$check_number = $request->check_number;
			$post_dated = $request->check_post_dated;
			$check_date = $request->check_date;

			$mo = $request->mo;
			$mo_val = $request->mo_val;
			$mo_ref = $request->mo_ref;

			$credit = $request->credit;
			$credit_val = $request->credit_val;
			$transac_id = $request->transac_id;
			$register_id = $request->register_id;

			$settlement_amount = bcsub($cash, $change, 6);

			$time = time();

			$breakdown = "CASH|".$cash_value."\n";
			if($post_dated) {
				$breakdown .= "PCHECK|".$check_val."\n";
			} else {
				$breakdown .= "CHECK|".$check_val."\n";
			}
			$breakdown .= "MO|".$mo_val."\n";
			$breakdown .= "CC|".$credit_val."\n";
			$breakdown .= "CHANGE|".$change;

			$breakdown_set = false;

			if($cuid) {
				$pending_usages = db_query("SELECT u.cuid, (credit_amount-IFNULL(pre.paid, 0)) AS credit_amount FROM user_term_credits_usages u
						LEFT JOIN (SELECT cuid, SUM(settlement_amount) AS paid FROM user_term_credits_settlements GROUP BY cuid) pre ON u.cuid = pre.cuid  
						WHERE u.uid = '%d' AND settled = '0' AND u.credit_amount > 0", $cuid);
				while($usage = db_fetch_object($pending_usages)) {
					if($usage->credit_amount > 0) {
						if($settlement_amount <= 0) {
							break;
						} else if($settlement_amount > 0 && bccomp($settlement_amount, $usage->credit_amount, 2) == -1) {
							if($breakdown_set) {
								$remarks = '';
							} else {
								$remarks = $breakdown;
								$breakdown_set = true;
							}
							db_query("INSERT INTO {user_term_credits_settlements}
								(cuid, settlement_amount, payment_type, remarks, settlement_date, euid, register_id)
								VALUES ('%d', '%s', '%s', '%s', '%s', '%d', '%s')", $usage->cuid, $settlement_amount, 'pos_payment', $remarks, $time, $uid, $register_id);
							if(!empty($remarks)) {
								$sid = db_last_insert_id('user_term_credits_settlements', 'sid');
								db_query("INSERT INTO {user_term_credits_cash_log} (sid, amount_paid, `change`, total) VALUES ('%d', '%s', '%s', '%s')", $sid, $cash_value, $change, $cash);
								if($check) {
									db_query("INSERT INTO {user_term_credits_check_log} (sid, paid_amount, check_number, post_dated, cash_date) VALUES ('%d', '%s', '%s', '%d', '%s')", $sid, $check_val, $check_number, $post_dated, $check_date);
								}
								if($credit) {
									db_query("INSERT INTO {user_term_credits_credit_card_log} (sid, amount_paid, transaction_id) VALUES ('%d', '%s', '%s')", $sid, $credit_val, $transac_id);
								}
								if($mo) {
									db_query("INSERT INTO {user_term_credits_money_order_log} (sid, paid_amount, reference) VALUES ('%d', '%s', '%s')", $sid, $mo_val, $mo_ref);
								}
							}
							break;
						} else {
							if($breakdown_set) {
								$remarks = '';
							} else {
								$remarks = $breakdown;
								$breakdown_set = true;
							}
							db_query("INSERT INTO {user_term_credits_settlements}
								(cuid, settlement_amount, payment_type, remarks, settlement_date, euid, register_id)
								VALUES ('%d', '%s', '%s', '%s', '%s', '%d', '%s')", $usage->cuid, $usage->credit_amount, 'pos_payment', $remarks, $time, $uid, $register_id);
							db_query("UPDATE {user_term_credits_usages} SET settled = '1' WHERE cuid = '%d'", $usage->cuid);
							$settlement_amount = bcsub($settlement_amount, $usage->credit_amount, 6);
							if(!empty($remarks)) {
								$sid = db_last_insert_id('user_term_credits_settlements', 'sid');
								db_query("INSERT INTO {user_term_credits_cash_log} (sid, amount_paid, `change`, total) VALUES ('%d', '%s', '%s', '%s')", $sid, $cash_value, $change, $cash);
								if($check) {
									db_query("INSERT INTO {user_term_credits_check_log} (sid, paid_amount, check_number, post_dated, cash_date) VALUES ('%d', '%s', '%s', '%d', '%s')", $sid, $check_val, $check_number, $post_dated, $check_date);
								}
								if($credit) {
									db_query("INSERT INTO {user_term_credits_credit_card_log} (sid, amount_paid, transaction_id) VALUES ('%d', '%s', '%s')", $sid, $credit_val, $transac_id);
								}
								if($mo) {
									db_query("INSERT INTO {user_term_credits_money_order_log} (sid, paid_amount, reference) VALUES ('%d', '%s', '%s')", $sid, $mo_val, $mo_ref);
								}
							}
						}
					}
				}

				pos_api_expose_respond(
				array("status" => true,
					  "message" => "Payment was made successfully.",
					  "cuid" => strval($cuid),
					  "time" => strval($time))
				, true);
			}
		}
	}

	pos_api_expose_respond($failed_response);
}

function _pos_api_expose_stock_adjustment($ticketId) {
	$products = db_query("SELECT nid, qty, price FROM {uc_order_products} WHERE order_id = '%d'", $ticketId);

	while($product = db_fetch_object($products)) {
		db_query("INSERT INTO {pos_out_stock_management} (product_nid, stock_adjustment, unit_price, `timestamp`) VALUES ('%d', '%d', '%s', '%s')", $product->nid, $product->qty, $product->price, time());
	}
}

function pos_api_expose_credit_checkout() {
	$failed_response = array(
		'status' => false,
		'message' => 'Could not checkout ticket, please check network and try again.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$ticketId = $request->ticketId;
		$total = pos_api_expose_get_order_total($ticketId);
		$cuid = $request->customer;
		$term_limit = $request->term_limit;
		$register_id = $request->register_id;
		$due = ((int)$term_limit*86400)+time();

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			db_query("INSERT INTO {user_term_credits_usages} (uid, order_id, credit_amount, term_limit, consumption_date, due_date, settled)
				VALUES ('%d', '%d', '%s', '%d', '%s', '%s', '%d')", $cuid, $ticketId, $total, $term_limit, time(), $due, 0);
			$credit_usage_id = db_last_insert_id('user_term_credits_usages', 'cuid');
			$sql = "INSERT INTO {pos_api_expose_transaction_log} (uid, cuid, ticket_id, amount, credit_usage_id, register_id, `timestamp`)
				VALUES ('%d', '%d', '%d', '%s', '%d', '%s', '%s')";
			db_query($sql, $uid, $cuid, $ticketId, $total, $credit_usage_id, $register_id, time());
			_pos_api_expose_stock_adjustment($ticketId);
			pos_api_expose_respond(array('status' => true, 'message' => 'Purchase complete.'), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_rma_create_ticket() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not create RMA ticket, please check network and try again.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			$order = new stdClass();
			$order->uid = $request->customer_uid;
			$order->order_status = 'pos_return'; //Open New ticket as a quote ticket
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

			foreach($request->products as $product) {
				$order_product = new stdClass();
				$order_product->order_id = $order->order_id;
				$order_product->nid = $product->nid;
				$order_product->title = $product->name;
				$order_product->model = $product->sku;
				$order_product->price = $product->price;

				$order_product->cost = pos_stock_management_average_cost($order_product->nid);
				if(empty($order_product->cost)) {
					//Use default automagic cost.
					$order_product->cost = db_result(db_query("SELECT cost FROM {uc_products} WHERE nid = '%d'", $order_product->nid));
				}

				$order_product->manufacturer = '';
				$order_product->weight = 0;
				$order_product->qty = $product->qty_returned;
				$order_product->data = array(
					'attributes' => array()
				);

				drupal_write_record('uc_order_products', $order_product);
			}

			pos_api_expose_respond(
				array("status" => true,
					  "ticketId" => $order->order_id,
					  "ticketStatus" => 'pos_return',
					  "message" => "RMA ticket created.")
				, true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_rma_refund() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not checkout process refund, please check network and try again.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$ticketId = $request->ticketId;
		$total = $request->total;
		$cuid = $request->customer_uid;
		$products = $request->products;
		$register_id = $request->register_id;

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			db_query("INSERT INTO {user_term_credits_usages} (uid, order_id, credit_amount, term_limit, consumption_date, due_date, settled) VALUES ('%d', '%d', '-%s', '0', '%s', '%s', '0')", $cuid, $ticketId, $total, time(), time());
			$credit_usage_id = $rma_pid = db_last_insert_id('user_term_credits_usages', 'cuid');
			db_query("INSERT INTO {pos_api_expose_rma_refund_log} (uid, cuid, credit_usage_id, total_refund, `timestamp`)
				VALUES ('%d', '%d', '%d', '%s', '%s')", $uid, $cuid, $credit_usage_id, $total, time());
			$rma_pid = db_last_insert_id('pos_api_expose_rma_refund_log', 'pid');

			db_query("INSERT INTO {pos_api_expose_transaction_log} (uid, cuid, amount, rma_pid, register_id, `timestamp`) VALUES ('%d', '%d', '%s', '%d', '%s', '%s')", $uid, $cuid, $total, $rma_pid, $register_id, time());

			if($products) {
				foreach($products as $product) {
					if($product->historic) {
						db_query("INSERT INTO {pos_api_expose_rma_returned_products} (order_product_id, qty_returned, refund_log_pid, price) VALUES ('%d', '%d', '%d', '%s')", $product->order_product_id, $product->qty_returned, $rma_pid, $product->price);

						db_query("INSERT INTO {pos_out_stock_management} (product_nid, stock_adjustment, unit_price, `timestamp`) VALUES ('%d', '%d', '%s', '%s')", $product->nid, (-1)*$product->qty_returned, $product->price, time());
					} else {
						db_query("INSERT INTO {pos_api_expose_rma_returned_products} (order_product_id, qty_returned, refund_log_pid, price) VALUES ('%d', '%d', '%d', '%s')", 0, $product->qty_returned, $rma_pid, $product->price);

						db_query("INSERT INTO {pos_out_stock_management} (product_nid, stock_adjustment, unit_price, `timestamp`) VALUES ('%d', '%d', '%s', '%s')", $product->order_product_id, (-1)*$product->qty_returned, $product->price, time());
					}
				}
			}

			pos_api_expose_respond(array('status' => true, 'message' => 'Refund complete. Amount was credited to customer account.'), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_ticket_update_status() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not update status on ticket.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$ticketId = $request->ticketId;
		$ticketStatus = $request->ticketStatus;

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			//Trigger tax calculation on server.
			if($ticketStatus == 'pos_completed') {
				pos_api_expose_ticket_update_total_on_server($ticketId);
			}

			db_query("UPDATE {uc_orders} SET order_status = '%s' WHERE order_id = '%d'", $ticketStatus, $ticketId);
			pos_api_expose_respond(array('status' => true,
										 'message' => 'Ticket status updated.'), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_ticket_invoice($order_id) {
	//Avoid Caching Invoice
	header('Cache-Control: no-cache, no-store, must-revalidate'); // HTTP 1.1.
	header('Pragma: no-cache'); // HTTP 1.0.
	header('Expires: 0'); // Proxies.

	$token = $_GET['token'];
	$uid = _pos_api_expose_uid_from_token($token);
	if($uid) {
		module_load_include('inc', 'uc_order', 'uc_order.admin');
		$order = uc_order_load($order_id);
		uc_order_invoice($order, 'print');
	} else {
		print "Access denied to invoice.";
	}
}

function pos_settlement_reciept($cuid, $timestamp) {
	//Avoid Caching Invoice
	header('Cache-Control: no-cache, no-store, must-revalidate'); // HTTP 1.1.
	header('Pragma: no-cache'); // HTTP 1.0.
	header('Expires: 0'); // Proxies.

	$token = $_GET['token'];
	$uid = _pos_api_expose_uid_from_token($token);
	if($uid) {
		$payment = db_fetch_object(db_query("SELECT cs.sid, cs.settlement_date, SUM(cs.settlement_amount) AS paid, cs.remarks
							FROM user_term_credits_settlements cs
							INNER JOIN user_term_credits_usages cu ON cs.cuid = cu.cuid
							WHERE cu.uid = '%d' AND cs.settlement_date = '%s'", $cuid, $timestamp));
		if($payment->sid) {
			$payments = array();

			$payments_breakdown = db_query("SELECT cs.sid, cs.settlement_amount, cu.credit_amount, cu.order_id, (cs.settlement_date-cu.consumption_date) AS days, (cu.credit_amount-IFNULL(pre.paid, 0)) AS balance
								FROM user_term_credits_settlements cs
								INNER JOIN user_term_credits_usages cu ON cs.cuid = cu.cuid
	                                        LEFT JOIN (SELECT cuid, SUM(settlement_amount) AS paid FROM user_term_credits_settlements GROUP BY cuid) pre ON cu.cuid = pre.cuid 
								WHERE cu.uid = '%d' AND cs.settlement_date = '%s'", $cuid, $timestamp);

			while($partial = db_fetch_object($payments_breakdown)) {
				$payments[] = $partial;
			}

			$customer = user_load($cuid);

			print theme('settlement_reciept', $payment, $payments, $customer);
		} else {
			print "No payment was found for customer.";
		}
	} else {
		print "Access denied to reciept.";
	}
}

function pos_api_expose_manager_override() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not override product price.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$ticketId = $request->ticketId;
		$productNid = $request->productNid;
		$qty_split = $request->qty_split;
		$unit_price = $request->unit_price;

		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			//Trigger tax calculation on server.
			db_query("INSERT INTO pos_api_expose_manager_override_log (uid, ticket_id, product_nid, qty_split, unit_price, `timestamp`) VALUES ('%d', '%d', '%d', '%d', '%s', '%s')", $uid, $ticketId, $productNid, $qty_split, $unit_price, time());
			db_query("UPDATE uc_order_products SET price = '%s' WHERE order_id ='%d' AND order_product_id = '%d'", $unit_price, $ticketId, $productNid);
			pos_api_expose_respond(array('status' => true,
										 'message' => 'Manager override updated.'), true);
		}
	}

	pos_api_expose_respond($failed_response);
}