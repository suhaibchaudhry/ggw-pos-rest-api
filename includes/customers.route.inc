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

function pos_api_expose_customer_search($token, $query) {
	if(!empty($token)) {
		$uid = db_result(db_query("SELECT paet.uid FROM {pos_api_expose_tokens} paet WHERE paet.token = '%s'", $token));

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
				$dataset[] = array(
					'id' => $item->uid,
					'account_id' => $item->name,
					'company_name' => $item->title,
					'company_phone' => $item->field_profile_company_phone_number,
					'agent_phone' => $item->field_profile_phone_number,
					'company_address' => $item->company_address,
					'roles' => _pos_api_expose_get_roles($item->uid)
				);
			}

			pos_api_expose_respond($dataset, true);
		}
	}

	pos_api_expose_respond(array());
}