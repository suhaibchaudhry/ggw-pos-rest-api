<?php
/*Payment invoice reciept*/
$uid = $customer->uid;
if($uid) {
  $profile = content_profile_load('profile', $uid);
}
?>
<!DOCTYPE HTML>
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
  <link rel="stylesheet" type="text/css" href="<?php print base_path() . drupal_get_path('theme', 'ggw_backend'); ?>/css/invoice.css" media="all" />
  <title><?php print $profile->title ?> (<?php print $profile->name ?>)</title>
  <style type="text/css" media="all">
  body {
    -webkit-print-color-adjust:exact;
  }
  table td.numeric-item {
    text-align: right;
  }
  table td.qty-item {
    text-align: center;
  }
  table th.left-item {
    text-align: left;
  }
  .cust-address {
  	width: 100%;
  }
  .cust-address span.label {
  	margin: 0 0 1em 0;
  }
  table tr td {
  	text-align: center;
  }
  .cust-address {
    margin: 0 0 2em;
  }
  </style>
</head>
<body>
  <div class="invoice">
    <div class="header">
      <div class="logo column"></div>
      <div class="address column">
        <p>8000 Harwin dr. Suite #200 Houston, TX 77036</p>
        <p><span class="label">Email: </span>info@general-goods.com</p>
        <p><span class="label">Website: </span>info@general-goods.com</p>
        <p><span class="label">Phone: </span>713-780-3636 <span class="label">Fax: </span>713-780-1718</p>
        <?php if($profile->field_tobacco_permit_id[0]['value']) : ?><p><span class="label">Tobacco ID: </span>93044639</p><?php endif; ?>
      </div>
      <div class="invoice-info column">
        <p class="invoiceid" style="font-size: 1.5em;"><span class="label">Payment Receipt</span></p>
        <p class="invoiceid"><span class="label">Reciept #:</span> <span class="value"><?php echo $payment->sid ?></span></p>
        <p class="invoicedate"><span class="label">Reciept Date:</span> <span class="value"><?php echo date("n/j/Y g:ia", $payment->settlement_date); ?></span></p>
      </div>
    </div>

    <div class="customer-info box">
      <div class="header-line"></div>
      <div class="billing-address cust-address">
        <span class="label">Customer:</span>
        <span class="value">
          <?php if($uid) { ?>
          <?php print $profile->title ?> (<?php print $profile->name ?>)<br />
          <?php print $profile->field_contact_remarks[0]['value'] ?><br />
          <?php print $profile->field_company_address[0]['street1'] ?><br />
          <?php print $profile->field_company_address[0]['city'] ?>, <?php print $profile->field_company_address[0]['state'] ?> <?php print $profile->field_company_address[0]['zip'] ?><br />
          <?php } else { ?>
            Walk-in Customer
          <?php } ?>
        </span>
      </div>
    </div>

    <div class="products">
      <div class="total-balance" style="text-align: right; margin-bottom: 1em; font-size: 1.4em;"><strong>Total Remaining Balance: </strong><?php print $customer->credit_limits->pending_payments_view ?></div>
      <?php
      $total_payments_made = 0;
      $header = array('#', 'Payment Date', 'Payment', 'Invoice #', 'Invoiced Amount', 'Settlement', 'Remaining Balance', 'Days');
      $rows = array();
      foreach($breakdown as $line_item) {
      	/*if($line_item->settlement_amount > 0 && $line_item->settlement_amount < .01) {
      		$line_item->settlement_amount = .01;
      	}*/
        $total_payments_made += $line_item->settlement_amount;
      	$rows[] = array($line_item->sid, date("n/j/Y", $payment->settlement_date), user_term_credits_recursive_round($payment->paid), $line_item->order_id, uc_currency_format($line_item->credit_amount, false), uc_currency_format($line_item->settlement_amount, false), uc_currency_format($line_item->balance, false), (int)($line_item->days/86400));
      }

      print theme('table', $header, $rows);
      ?>
    </div>

    <div class="line-items">
    <div class="total-balance" style="text-align: right; margin-bottom: 1em; font-size: 1.4em;"><strong>Total Remaining Balance: </strong><?php print $customer->credit_limits->pending_payments_view ?></div>
    <?php
    $items = split("\n", $payment->remarks);
    $payment_forms = array();
    foreach($items as $item) {
    	$components = split("\|", $item);
    	$payment_forms[$components[0]] = $components[1];
    }

    $rows = array();
    if($payment_forms['CASH'] > 0 || $payment_forms['CHANGE'] > 0) {
    	$rows[] = array('CASH', uc_currency_format($payment_forms['CASH'], false));
    	$rows[] = array('CHANGE', uc_currency_format($payment_forms['CHANGE'], false));
    }

    if($payment_forms['CHECK'] > 0) {
      $check_info = db_fetch_object(db_query("SELECT cash_date, check_number FROM user_term_credits_check_log WHERE sid = '%d'", $payment->sid));
    	$content = '';

      if($check_info->check_number) {
        $content .= ' - '.$check_info->check_number;
      }

      if($check_info->cash_date) {
        $content .= ' - '.$check_info->cash_date;
      }

      $rows[] = array('CHECK'.$content, uc_currency_format($payment_forms['CHECK'], false));
    }

    if($payment_forms['PCHECK'] > 0) {
      $check_info = db_fetch_object(db_query("SELECT cash_date, check_number FROM user_term_credits_check_log WHERE sid = '%d'", $payment->sid));
      $content = '';

      if($check_info->check_number) {
        $content .= ' - '.$check_info->check_number;
      }

      if($check_info->cash_date) {
        $content .= ' - '.$check_info->cash_date;
      }

      $rows[] = array('POST DATED CHECK'.$content, uc_currency_format($payment_forms['PCHECK'], false));
    }

    if($payment_forms['MO'] > 0) {
      $content = '';
      $reference = db_result(db_query("SELECT reference FROM user_term_credits_money_order_log WHERE sid = '%d'", $payment->sid));
      if($reference) {
        $content .= ' - '.$reference;
      }
    	$rows[] = array('MONEY ORDER'.$content, uc_currency_format($payment_forms['MO'], false));
    }

    if($payment_forms['CC'] > 0) {
      $content = '';
      $transac_id = db_result(db_query("SELECT transaction_id FROM user_term_credits_credit_card_log WHERE sid = '%d'", $payment->sid));
      if($transac_id) {
        $content .= ' - '.$transac_id;
      }
    	$rows[] = array('CREDIT CARD'.$content, uc_currency_format($payment_forms['CC'], false));
    }

    if($payment_forms['RMA'] > 0) {
      $rows[] = array('RMA DEBIT', uc_currency_format($payment_forms['RMA'], false));
    }

    print theme('table', array(), $rows);
    ?>
    </div>
    <div class="payment-details">
      <?php print theme('table', array(), array(
        array('<strong>Total Payment</strong>', uc_currency_format($total_payments_made)),
        array('<strong>Starting Balance</strong>', uc_currency_format($customer->credit_limits->pending_payments+$total_payments_made)),
        array('<strong>Ending Balance</strong>', $customer->credit_limits->pending_payments_view)
      )); ?>
    </div>
  </div>
</body>
</html>
