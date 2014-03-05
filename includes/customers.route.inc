<?php
function _pos_api_expose_get_roles($uid) {
	$user_roles = array();
	$sql = "SELECT rid FROM {users_roles} ur WHERE ur.uid = '%d'";
	$roles = db_query($sql, $uid);
	while($role = db_fetch_object($roles)) {
		$user_roles[] = $role->rid; 
	}

	return $user_roles;
}

function _pos_api_expose_order_history($user) {
  $header = array(
    array('data' => t('Date'), 'field' => 'o.created', 'sort' => 'desc'),
    array('data' => t('Order #'), 'field' => 'o.order_id'),
    array('data' => t('Status'), 'field' => 'os.title'),
    array('data' => t('Products'), 'field' => 'products'),
    array('data' => t('Total'), 'field' => 'o.order_total')
  );

  $rows = array();

  $context = array(
    'revision' => 'themed-original',
    'type' => 'amount',
  );
  $list = uc_order_status_list('general', TRUE);
  $result = pager_query("SELECT o.order_id, o.uid, o.created, os.title, SUM(op.qty) AS products, o.order_total AS total, o.order_status FROM {uc_orders} AS o LEFT JOIN {uc_order_statuses} AS os ON o.order_status = os.order_status_id LEFT JOIN {uc_order_products} AS op ON o.order_id = op.order_id WHERE o.uid = %d AND o.order_status IN ('pos_in_progress', 'pos_completed', 'pos_canceled', 'pos_quote', 'pos_invoiced', 'processing') GROUP BY o.order_id, o.uid, o.created, os.title, o.order_total, o.order_status". tablesort_sql($header), 20, 0, "SELECT COUNT(*) FROM {uc_orders} WHERE uid = %d AND order_status NOT IN ". uc_order_status_list('specific', TRUE), $user->uid);

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
      array('data' => check_plain($order->title)),
      array('data' => (!is_null($order->products) ? $order->products : 0), 'align' => 'center'),
      array('data' => uc_price($order->total, $context), 'align' => 'right'),
    );
  }

  if (empty($rows)) {
    $rows[] = array(array('data' => t('No orders available.'), 'colspan' => 5));
  }

  $content = theme('table', $header, $rows, array('class' => 'pure-table uc-order-history'))
       . theme('pager', NULL, 20, 0);

  return str_replace('src="/misc/', 'src="images/misc/', $content);
}

function pos_api_expose_customer_invoices() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not get customer invoices.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$customer_uid = $request->customer_uid;
		$uid = _pos_api_expose_uid_from_token($request->token);

		if($uid) {
			if($customer_uid) {
				$customer = user_load($customer_uid);
				$invoices = _pos_api_expose_order_history($customer);
				pos_api_expose_respond(array('status' => true, 'invoices' => $invoices), true);
			}
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_customer_info() {
	module_load_include('inc', 'user', 'user.pages');

	$failed_response = array(
		'status' => false,
		'error' => 'Could not get customer info.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$customer_uid = $request->customer_uid;
		$uid = _pos_api_expose_uid_from_token($request->token);
		if($uid) {
			if($customer_uid) {
				$customer = user_load($customer_uid);
				$customer_info = user_view($customer);
				$invoices = _pos_api_expose_order_history($customer);
				$payments = utc_credit_report($customer);
				pos_api_expose_respond(array('status' => true, 'customer_info' => $customer_info, 'invoices' => $invoices, 'payments' => $payments), true);
			}
		}
	}

	pos_api_expose_respond($failed_response);
}

function _pos_api_expose_map_customer_datum($item) {
	return array(
				'id' => $item->uid,
				'account_id' => $item->name,
				'company_name' => $item->title,
				'company_phone' => $item->field_profile_company_phone_number,
				'agent_phone' => $item->field_profile_phone_number,
				'company_address' => $item->company_address,
				'roles' => _pos_api_expose_get_roles($item->uid)
			);
}

function pos_api_expose_get_customer() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not load customer.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$token = $request->token;
		$customerUid = $request->customerUid;

		$uid = _pos_api_expose_uid_from_token($token);
		if($uid) {
			$sql = "SELECT u.uid, n.title, u.name,

					ctp.field_profile_company_phone_number,
					ctp.field_profile_phone_number,

					CONCAT(ctp.field_company_address_street1, ' ',
					ctp.field_company_address_street2, '\n',
					ctp.field_company_address_city, ', ',
					ctp.field_company_address_state, ' ',
					ctp.field_company_address_zip) AS company_address

					FROM node n
					INNER JOIN users u ON n.uid = u.uid
					INNER JOIN content_type_profile ctp ON ctp.vid = n.vid
					WHERE u.uid = '%d' AND n.type = 'profile'";

			$item = db_fetch_object(db_query($sql, $customerUid));
			$user = _pos_api_expose_map_customer_datum($item);

			pos_api_expose_respond(array('status' => true, 'customer' => $user), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_customer_search($token, $query) {
	if(!empty($token)) {
		$uid = _pos_api_expose_uid_from_token($token);

		if($uid) {
			//By Search Company Name with Fluid Revelevance
			$fulltext_sql = "SELECT nv.vid, MATCH(nv.title) AGAINST ('%s') AS relevance
							 FROM {node_revisions} nv
							 HAVING relevance > 0";

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

			//Search by Address
			$address_search = "SELECT ctp.vid, 5 AS relevance
							   FROM content_type_profile ctp
							   WHERE
							   CONCAT(ctp.field_company_address_street1, ' ',
							   ctp.field_company_address_street2) LIKE '%%%s%%'
							   OR ctp.field_company_address_zip LIKE '%s%%'";

			$sql = "SELECT u.uid, n.title, u.name,

					ctp.field_profile_company_phone_number,
					ctp.field_profile_phone_number,

					CONCAT(ctp.field_company_address_street1, ' ',
					ctp.field_company_address_street2, '\n',
					ctp.field_company_address_city, ', ',
					ctp.field_company_address_state, ' ',
					ctp.field_company_address_zip) AS company_address

					FROM node n
					INNER JOIN users u ON n.uid = u.uid
					INNER JOIN content_type_profile ctp ON ctp.vid = n.vid
					INNER JOIN ((".$fulltext_sql.") UNION
					(".$accountid_search.") UNION
					(".$phone_searches.") UNION (".$address_search."))
					search ON search.vid = n.vid
					WHERE n.type = 'profile'
					ORDER BY search.relevance DESC
					LIMIT 0, 8";

			$dataset = array();
			$items = db_query($sql, $query, $query, $query, $query, $query, $query);

			while($item = db_fetch_object($items)) {
				$dataset[] = _pos_api_expose_map_customer_datum($item);
			}

			pos_api_expose_respond($dataset, true);
		}
	}

	pos_api_expose_respond(array());
}