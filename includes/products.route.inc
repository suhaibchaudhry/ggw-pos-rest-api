<?php
function _pos_api_expose_load_special_prices($vid, $sale_object) {
	$prices = array();
	$role_prices = db_query("SELECT rid, price FROM {uc_price_per_role_prices} WHERE vid = '%d'", $vid);
	while($role_price = db_fetch_object($role_prices)) {
		
		_pos_api_expose_check_role_price($sale_object->sell_price, $sale_object->field_level_1_price_value, $role_price->rid, "12", $role_price->price);
		_pos_api_expose_check_role_price($sale_object->sell_price, $sale_object->field_level_2_price_value, $role_price->rid, "13", $role_price->price);
		_pos_api_expose_check_role_price($sale_object->sell_price, $sale_object->field_level_3_price_value, $role_price->rid, "14", $role_price->price);
		_pos_api_expose_check_role_price($sale_object->sell_price, $sale_object->field_level_4_price_value, $role_price->rid, "15", $role_price->price);


		$prices[] = array('role' => $role_price->rid, 'price' => $role_price->price);
	}

	return $prices;
}

function _pos_api_expose_check_role_price($sale_price, $level_price, $rid, $cmp_rid, &$price) {
	if($rid == $cmp_rid) {
		if(!empty($level_price)) {
			$price = $level_price;
		} else if(!empty($sale_price)) {
			$price = $sale_price;
		}
	}
}

function _pos_api_expose_get_category($nid) {
	return db_result(db_query("SELECT gsrt.invoice_category FROM {term_node} tn INNER JOIN {ggw_state_reporting_terms} gsrt ON tn.tid = gsrt.vid WHERE tn.nid = '%d'", $nid));
}

function _pos_api_expose_map_product_datum($item, $special_price = true, $ticketId = FALSE, $unhistoric = true) {
	if(empty($item)) {
		$item = new stdClass();
	}

	if(empty($item->filepath)) {
		$item->filepath = 'sites/general-goods.com/files/imagefield_default_images/notfound_0.png';
	}

	$product = array(
		'id' => $item->nid,
		'sku' => $item->model,
		'list_price' => $item->list_price,
		'packaging' => $item->field_prod_packing_value,
		'sell_price' => $item->sell_price,
		'name' => trim($item->title),
		'image' => imagecache_create_url('pos_client_search', $item->filepath),
		'thumbnail' => imagecache_create_url('uc_thumbnail', $item->filepath)
	);

	if($item->order_product_id) {
		$product['order_product_id'] = $item->order_product_id;

		if($ticketId) {
			$price = db_fetch_object(db_query("SELECT mol.unit_price, mol.qty_split FROM {pos_api_expose_manager_override_log} mol WHERE mol.ticket_id = '%d' AND mol.product_nid = '%d' ORDER BY `timestamp` DESC", $ticketId, $item->order_product_id));
			if($price) {
				$product['manager_price'] = $price->unit_price;
				$product['qty_split'] = $price->qty_split;
			}
		}
	}

	$sale_object = _pos_api_expose_get_sale_object($item->vid);

	if(!empty($sale_object->sell_price)) {
		$product['sell_price'] = $sale_object->sell_price;
	}

	if($special_price) {
		$product['special_prices'] = _pos_api_expose_load_special_prices($item->vid, $sale_object);
	}

	$category = _pos_api_expose_get_category($item->nid);
	if($category) {
		$product['category'] = $category;
	}

	if(!empty($item->qty)) {
		$product['qty'] = (int)$item->qty;
	}

	$product['unhistoric'] = $unhistoric;

	return $product;
}

function _pos_api_expose_get_sale_object($vid) {
  return db_fetch_object(
  			db_query("SELECT ctp.field_sale_period_value,
  							 ctp.field_sale_period_value2,
  							 ctp.field_sale_max_units_value,
  							 ctp.field_sale_price_value,
  							 ctp.field_level_1_price_value,
  							 ctp.field_level_2_price_value,
  							 ctp.field_level_4_price_value,
  							 ctp.field_level_3_price_value
  					  FROM content_type_product ctp
  					  WHERE ctp.vid = '%d' AND ctp.field_sale_period_value < NOW() AND ctp.field_sale_period_value2 > NOW()
  					  ORDER BY ctp.vid DESC", $vid
  			)
  		  );
}

function _pos_api_expose_map_product_rma($item, $unhistoric = false) {
	if(empty($item->filepath)) {
		$item->filepath = 'sites/general-goods.com/files/imagefield_default_images/notfound_0.png';
	}

	$product = array(
		'date' => format_date($item->created, 'custom', variable_get('uc_date_format_default', 'm/d/Y')),
		'ticketId' => $item->order_id,
		'id' => $item->order_product_id,
		'nid' => $item->nid,
		'sku' => $item->model,
		'packaging' => $item->field_prod_packing_value,
		'sell_price' => $item->price,
		'name' => trim($item->title),
		'image' => imagecache_create_url('pos_client_search', $item->filepath),
		'thumbnail' => imagecache_create_url('uc_thumbnail', $item->filepath),
		'qty' => (int)$item->qty,
		'remaining_qty' => $item->remaining_qty
	);

	if($item->remaining_qty) {
		$product['remaining_qty'] = $item->remaining_qty;
	} else {
		$product['remaining_qty'] = $item->qty;
	}

	$product['unhistoric'] = $unhistoric;

	/*
	$category = _pos_api_expose_get_category($item->nid);
	if($category) {
		$product['category'] = $category;
	}*/

	return $product;
}

function pos_api_rma_process() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not process RMA request.'
	);
	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$uid = _pos_api_expose_uid_from_token($request->token);
		$item_barcode = $request->item_barcode;
		$customer_uid = $request->customer_uid;

		if($uid) {
			$additional_barcode_sql = "SELECT cfab.nid FROM {content_field_additional_barcodes} cfab WHERE cfab.field_additional_barcodes_value = '%s'";

			$sku_sql = "SELECT p.nid
						FROM {uc_products} p
						LEFT JOIN {content_type_product} ctp ON p.nid = ctp.nid
						WHERE p.model = '%s' OR
							  ctp.field_prod_unit_barcode_value = '%s' OR
							  p.nid IN (".$additional_barcode_sql.")";

			$items = db_query("SELECT uop.order_product_id, uo.created, uop.order_id, uop.nid, uop.title, uop.model, uop.price, uop.qty, ctp.field_prod_packing_value, f.filepath, (uop.qty-IFNULL(rma.qty_returned,0)) AS remaining_qty
				FROM {uc_order_products} uop
				INNER JOIN {uc_orders} uo ON uop.order_id = uo.order_id
				LEFT JOIN {content_type_product} ctp ON uop.nid = ctp.nid
				LEFT JOIN {content_field_image_cache} cfic ON uop.nid = cfic.nid
				LEFT JOIN {files} f ON f.fid = cfic.field_image_cache_fid
				LEFT JOIN (SELECT order_product_id, SUM(qty_returned) AS qty_returned FROM {pos_api_expose_rma_returned_products} GROUP BY order_product_id) rma ON uop.order_product_id = rma.order_product_id
				WHERE uo.uid = '%d' AND uop.nid IN (".$sku_sql.") AND uo.order_status = 'pos_completed' AND (uop.qty > rma.qty_returned OR ISNULL(rma.qty_returned))", $customer_uid, $item_barcode, $item_barcode, $item_barcode);

			if($items) {
				$products = array();

				while($item = db_fetch_object($items)) {
					$price = db_fetch_object(db_query("SELECT mol.unit_price, mol.qty_split FROM {pos_api_expose_manager_override_log} mol WHERE mol.ticket_id = '%d' AND mol.product_nid = '%d' ORDER BY `timestamp` DESC", $item->order_id, $item->order_product_id));
					if($price) {
						$item->price = $price->unit_price;
						//$product['qty_split'] = $price->qty_split;
					}
					$products[] = _pos_api_expose_map_product_rma($item);
				}

				if(count($products) > 0) {
					$items2 = _pos_api_expose_rma_all_item_search($item_barcode);
					while($item = db_fetch_object($items2)) {
						$item->unhistoric = true;
						$products[] = _pos_api_expose_map_product_datum($item);
					}

					$response = array(
						'status' => true,
						'products' => $products
					);

					pos_api_expose_respond($response, true);
				} else {
					$products = array();
					$items = _pos_api_expose_rma_all_item_search($item_barcode);

					if($items) {
						while($item = db_fetch_object($items)) {
							$products[] = _pos_api_expose_map_product_datum($item);
						}

						if(count($products) > 0) {
							$failed_response = array(
								'status' => false,
								'error_forgive' => true,
								'products' => $products,
								'error' => 'Item was not found but not in customer history.'
							);
						} else {
							$failed_response = array(
								'status' => false,
								'error_forgive' => false,
								'error' => 'Item was not found in customer history and in products.'
							);
						}
					} else {
						$failed_response = array(
							'status' => false,
							'error_forgive' => false,
							'error' => 'Item was not found in customer history and in products.'
						);
					}
				}
			} else {
				$failed_response = array(
					'status' => false,
					'error_forgive' => false,
					'error' => 'Item was not found in customer history.'
				);
			}
		}
	}

	pos_api_expose_respond($failed_response);
}

function _pos_api_expose_rma_all_item_search($item_barcode) {
	$additional_barcode_sql = "SELECT cfab.nid FROM {content_field_additional_barcodes} cfab WHERE cfab.field_additional_barcodes_value = '%s'";

	$sku_sql = "SELECT p.nid
				FROM {uc_products} p
				LEFT JOIN {content_type_product} ctp ON p.nid = ctp.nid
				WHERE p.model = '%s' OR
				  ctp.field_prod_unit_barcode_value = '%s' OR
				  p.nid IN (".$additional_barcode_sql.")";


	$sql = 'SELECT n.nid, n.vid, n.title, p.model, p.list_price, p.sell_price, f.filepath, ctp_wrap.field_prod_packing_value
		FROM {uc_products} p
		INNER JOIN {node} n ON n.vid = p.vid

		LEFT JOIN {content_field_image_cache} cfic ON n.vid = cfic.vid
		LEFT JOIN {files} f ON f.fid = cfic.field_image_cache_fid
		LEFT JOIN {content_type_product} ctp_wrap ON n.vid = ctp_wrap.vid

		INNER JOIN ('.$sku_sql.') fts ON n.nid = fts.nid

		WHERE n.status > 0
		LIMIT 0, 48';

	return db_query($sql, $item_barcode, $item_barcode, $item_barcode);
}

function pos_api_expose_product_search($token) {
	if(!empty($token)) {
		$uid = _pos_api_expose_uid_from_token($token);
		if($uid) {
			$query = $_GET['searchQuery'];			

			$additional_barcode_sql = "SELECT cfab.vid FROM {content_field_additional_barcodes} cfab WHERE cfab.field_additional_barcodes_value LIKE '%s%%'";

			$sku_sql = "SELECT p.vid, 1 AS relevance
						FROM {uc_products} p
						LEFT JOIN {content_type_product} ctp ON p.vid = ctp.vid
						WHERE p.model LIKE '%s%%' OR
							  ctp.field_prod_unit_barcode_value LIKE '%s%%' OR
							  p.vid IN (".$additional_barcode_sql.")";

			$fulltext_sql = "SELECT nv.vid, MATCH(nv.title) AGAINST ('%s') AS relevance
							 FROM {node_revisions} nv
							 HAVING relevance > 0";

			$partial_sql = "SELECT nvv.vid, 50 AS relevance FROM {node_revisions} nvv WHERE LOWER(nvv.title) LIKE LOWER('%s%%')";

			//UNION ALL appears to be significantly faster than UNION.
			$sql = 'SELECT n.nid, n.vid, n.title, p.model, p.list_price, p.sell_price, f.filepath, ctp_wrap.field_prod_packing_value
					FROM {uc_products} p
					INNER JOIN {node} n ON n.vid = p.vid

					LEFT JOIN {content_field_image_cache} cfic ON n.vid = cfic.vid
					LEFT JOIN {files} f ON f.fid = cfic.field_image_cache_fid
					LEFT JOIN {content_type_product} ctp_wrap ON n.vid = ctp_wrap.vid

					INNER JOIN (('.$sku_sql.') UNION ALL ('.$fulltext_sql.') UNION ALL ('.$partial_sql.')) fts ON n.vid = fts.vid

					WHERE n.status > 0
					ORDER BY fts.relevance DESC, n.title ASC
					LIMIT 0, 48';

			$dataset = array();

			$items = db_query($sql, $query, $query, $query, $query, $query);
			while($item = db_fetch_object($items)) {
				$dataset[] = _pos_api_expose_map_product_datum($item);
			}

			pos_api_expose_respond($dataset, true);
		}
	}

	pos_api_expose_respond(array());
}

function pos_api_expose_load_ticket_products() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not load ticket products.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$token = $request->token;
		$ticketId = $request->ticketId;

		$uid = _pos_api_expose_uid_from_token($token);
		if($uid) {
			_pos_api_expose_log_ticket_load($uid, $ticketId);

			$sql = "SELECT n.nid, n.vid, n.title, p.model, p.list_price, uo.price AS sell_price, f.filepath, ctp_wrap.field_prod_packing_value, uo.qty, uo.order_product_id
			FROM {uc_order_products} uo

			INNER JOIN {uc_products} p ON p.nid = uo.nid
			INNER JOIN {node} n ON n.vid = p.vid

			LEFT JOIN {content_field_image_cache} cfic ON n.vid = cfic.vid
        		LEFT JOIN {files} f ON f.fid = cfic.field_image_cache_fid
        		LEFT JOIN {content_type_product} ctp_wrap ON n.vid = ctp_wrap.vid
        		WHERE order_id = '%d'

        	ORDER BY order_product_id ASC";

    		$items = db_query($sql, $ticketId);
			$dataset = array();
			$zone = db_result(db_query("SELECT billing_zone FROM uc_orders WHERE order_id = '%d'", $ticketId));
			while($item = db_fetch_object($items)) {
				$dataset[] = _pos_api_expose_map_product_datum($item, true, $ticketId);
			}

			pos_api_log_ticket_event($ticketId, $uid, 'Ticket opened by employee: ');

			pos_api_expose_respond(array('status' => true, 'zone' => $zone, 'products' => $dataset), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_rma_products() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not load return item products.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$token = $request->token;
		$ticketId = $request->ticketId;
		$total = 0;
		$uid = _pos_api_expose_uid_from_token($token);
		if($uid) {
			$sql = "SELECT n.nid, n.vid, n.title, p.model, p.list_price, uop.price AS sell_price, f.filepath, ctp_wrap.field_prod_packing_value, rp.qty_returned AS qty
					FROM {pos_api_expose_rma_returned_products} rp
					INNER JOIN {uc_order_products} uop ON rp.order_product_id = uop.order_product_id
					INNER JOIN {uc_products} p ON p.nid = uop.nid
					INNER JOIN {node} n ON n.vid = p.vid

					LEFT JOIN {content_field_image_cache} cfic ON n.vid = cfic.vid
					LEFT JOIN {files} f ON f.fid = cfic.field_image_cache_fid
					LEFT JOIN {content_type_product} ctp_wrap ON n.vid = ctp_wrap.vid

					WHERE uop.order_id = '%d'";

			$products = db_query($sql, $ticketId);
			$data = array();
			while($product = db_fetch_object($products)) {
				$total += $product->qty*$product->sell_price;
				$data[] = _pos_api_expose_map_product_datum($product, false);
			}

			/*
			$total = db_result(db_query("SELECT SUM(refunds.total_refund) FROM (SELECT DISTINCT rl.pid, uop.order_id, rl.total_refund FROM pos_api_expose_rma_refund_log rl
					INNER JOIN pos_api_expose_rma_returned_products rp ON rl.pid = rp.refund_log_pid
					INNER JOIN uc_order_products uop ON uop.order_product_id = rp.order_product_id) refunds WHERE refunds.order_id = '%d'", $ticketId));*/
			
			pos_api_expose_respond(array('status' => true, 'products' => $data, 'total' => $total), true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_product_scan() {
	$failed_response = array(
		'scan' => false,
		'error' => 'Could not scan barcode.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$token = $request->token;
		$barcode = trim($request->barcode);

		$sell_price = $request->sell_price;
		/*$rma_scan = $request->rma_scan;*/

		$uid = _pos_api_expose_uid_from_token($token);
		if($uid) {
			$additional_barcode_sql = "SELECT cfab.vid FROM {content_field_additional_barcodes} cfab WHERE cfab.field_additional_barcodes_value = '%s'";

			$sku_sql = "SELECT p.vid
						FROM {uc_products} p
						LEFT JOIN {content_type_product} ctp ON p.vid = ctp.vid
						WHERE p.model = '%s' OR
							  ctp.field_prod_unit_barcode_value = '%s' OR
							  p.vid IN (".$additional_barcode_sql.")";

			$sql = 'SELECT n.nid, n.vid, n.title, p.model, p.list_price, p.sell_price, f.filepath, ctp_wrap.field_prod_packing_value
					FROM {uc_products} p
					INNER JOIN {node} n ON n.vid = p.vid

					LEFT JOIN {content_field_image_cache} cfic ON n.vid = cfic.vid
					LEFT JOIN {files} f ON f.fid = cfic.field_image_cache_fid
					LEFT JOIN {content_type_product} ctp_wrap ON n.vid = ctp_wrap.vid

					WHERE p.vid IN ('.$sku_sql.') AND n.status > 0
					LIMIT 0, 1';

			$product = db_fetch_object(db_query($sql, $barcode, $barcode, $barcode));
			/*
			if($rma_scan) {
				$product->sell_price = $sell_price;
			}
			*/
			$datum = _pos_api_expose_map_product_datum($product);

			if($product) {
				$response = array(
					'scan' => true,
					'product' => $datum
				);
			} else {
				$response = array(
					'scan' => false,
					'error' => 'Could not find barcode.'
				);
			}

			pos_api_expose_respond($response, true);
		}
	}

	pos_api_expose_respond($failed_response);
}

function pos_api_expose_product_price_history() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not find products in customer history.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$token = $request->token;
		$uid = _pos_api_expose_uid_from_token($token);
		if($uid) {
			$product_id = $request->product_id;
			$cuid = $request->cuid;

			$products = array();

			$sql = "SELECT * FROM (SELECT ucp.order_product_id, uo.order_status, IFNULL(unit_price, ucp.price) AS price, ucp.cost, IFNULL(mol.qty_split, 1) AS qty_split, MAX(IFNULL(mol.timestamp, uo.modified)) AS price_date FROM uc_order_products ucp
			INNER JOIN {uc_orders} uo ON uo.order_id = ucp.order_id
			LEFT JOIN {pos_api_expose_manager_override_log} mol ON ucp.order_product_id = mol.product_nid
			WHERE ucp.nid IN (SELECT ucpi.nid FROM {uc_order_products} ucpi WHERE ucpi.order_product_id = '%d') AND uo.uid = '%d'
			GROUP BY price, qty_split, order_status
			ORDER BY price_date DESC) his WHERE his.order_status IN ('pos_completed', 'pos_return_closed') GROUP BY his.price, his.qty_split, his.order_status ORDER BY price_date DESC";

			$product_items = db_query($sql, $product_id, $cuid);

			while($product = db_fetch_object($product_items)) {
				$product->price_date = date("n/j/Y", $product->price_date);
				$products[] = $product;
			}

			if(count($products) > 0) {
				$response = array(
					'status' => true,
					'product' => $products
				);
			} else {
				$response = array(
					'status' => false,
					'error' => 'No product history was found.'
				);
			}

			pos_api_expose_respond($response, true);
		}
	}

	pos_api_expose_respond($failed_response);
}
