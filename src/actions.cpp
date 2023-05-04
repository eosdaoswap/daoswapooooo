void daoswap::_notify()
{
    //config_table _config( get_self(), get_self().value );
    //auto config = _config.get_or_default();
    //for ( const name notifier : config.notifiers ) {
    //    if ( is_account( notifier ) ) require_recipient( notifier );
    //}

    if(!has_auth(get_self()))
    {
      require_recipient( ACCOUNT_LOG );
    }
    else
    {
      require_recipient( ACCOUNT_NOTICE );
    }
}

void daoswap::liquiditylog(const symbol_code& pair_id, const name& owner, const name& action, const extended_asset& liquidity, const extended_asset& liquidity0,  const extended_asset& liquidity1, const asset& total_liquidity, const asset& reserve0, const asset& reserve1)
{
    //require_auth( get_self() );
    _notify();
    //require_recipient( owner );
}

void daoswap::swaplog(const symbol_code& pair_id, const name& owner, const name& action, const extended_asset& quantity_in, const extended_asset& quantity_out, const asset& protocol_fee,const asset& trade_fee, const double& trade_price, const asset& reserve0, const asset& reserve1 )
{
    //require_auth( get_self() );
    _notify();
    //require_recipient( owner );
}

void daoswap::tokenlog(const symbol_code& pair_id, const name& owner,const name& action,const extended_asset& liquidity,const uint64_t& exptime)
{
    //require_auth( get_self() );
    _notify();
    //require_recipient( owner );
}

void daoswap::pairidlog(const symbol_code& pair_id,const name& owner,const name& action)
{
    //require_auth( get_self() );
    _notify();
    //require_recipient( owner );
}

void daoswap::pledgeslog(const symbol_code& pair_id, const name& owner,const name& action,const uint64_t& pledgeid,const extended_asset& liquidity,const uint64_t& receivetime,const uint64_t& expiredtime,const uint64_t& cdsid)
{
    //require_auth( get_self() );
    _notify();
    //require_recipient( owner );
}

void daoswap::_liquiditylog(const symbol_code& pair_id, const name& owner, const name& action, const extended_asset& liquidity, const extended_asset& liquidity0,  const extended_asset& liquidity1, const asset& total_liquidity, const asset& reserve0, const asset& reserve1)
{
    eosio::action(
        permission_level{get_self(), PERMISSION_ACTION},
        get_self(),
        METHOD_LIQLOG,
        std::make_tuple(pair_id,owner,action,liquidity,liquidity0,liquidity1,total_liquidity,reserve0,reserve1))
    .send(); 
}

void daoswap::_swaplog(const symbol_code& pair_id, const name& owner, const name& action, const extended_asset& quantity_in, const extended_asset& quantity_out, const asset& protocol_fee,const asset& trade_fee, const double& trade_price, const asset& reserve0, const asset& reserve1 )
{
    eosio::action(
        permission_level{get_self(), PERMISSION_ACTION},
        get_self(),
        METHOD_SWAPLOG,
        std::make_tuple(pair_id,owner,action,quantity_in,quantity_out,protocol_fee,trade_fee,trade_price,reserve0,reserve1))
    .send(); 
}


void daoswap::_tokenlog(const symbol_code& pair_id, const name& owner,const name& action,const extended_asset& liquidity,const uint64_t& exptime)
{
    eosio::action(
        permission_level{get_self(), PERMISSION_ACTION},
        get_self(),
        METHOD_TOKENLOG,
        std::make_tuple(pair_id,owner,action,liquidity,exptime))
    .send();     
}

void daoswap::_pairidlog(const symbol_code& pair_id,const name& owner,const name& action)
{
    eosio::action(
        permission_level{get_self(), PERMISSION_ACTION},
        get_self(),
        METHOD_PAIRIDLOG,
        std::make_tuple(pair_id,owner,action))
    .send();     
}

void daoswap::_pledgeslog(const symbol_code& pair_id, const name& owner,const name& action,const uint64_t& pledgeid,const extended_asset& liquidity,const uint64_t& receivetime,const uint64_t& expiredtime,const uint64_t& cdsid)
{
    eosio::action(
        permission_level{get_self(), PERMISSION_ACTION},
        get_self(),
        METHOD_PLEDGELOG,
        std::make_tuple(pair_id,owner,action,pledgeid,liquidity,receivetime,expiredtime,cdsid))
    .send();     
}


void daoswap::init(const name& mgr_account,const symbol_code& pair_id )
{
    require_auth( get_self() );

    config_table _config( get_self(), get_self().value );
    auto config = _config.exists() ? _config.get() : config_row{};

    // token contract can not be modified once initialized
    check( is_account( mgr_account ), "600406:Invalid contract account!!");

    // set config
    config.mgr_account = mgr_account;
    config.pair_id = pair_id;
    _config.set( config, get_self() );
}


void daoswap::reset()
{
    require_auth( get_self() );

    config_table _config( get_self(), get_self().value );
    _config.remove();
}


void daoswap::setfee(const name& mgr,  const uint8_t& trade_fee, const std::optional<uint8_t> protocol_fee, const std::optional<name> fee_account )
{
    _verify_mgr(mgr);

    config_table _config( get_self(), get_self().value );
    check( _config.exists(), "600101:contract is under maintenance" );
    auto config = _config.get();    
    check(config.mgr_account == mgr,"600103:is not a valid contract manager");    

    // required params
    check( trade_fee <= MAX_TRADE_FEE, "600111:Transaction rate exceeds limit");
    check( *protocol_fee <= MAX_PROTOCOL_FEE, "600112:The mining pool rate exceeds the limit");

    // optional params
    if ( fee_account->value ) check( is_account( *fee_account ), "600406:Invalid contract account");
    if ( *protocol_fee ) check( fee_account->value, "600112:The mining pool rate exceeds the limit");

    // set config
    config.trade_fee = trade_fee;
    config.protocol_fee = *protocol_fee;
    config.fee_account = *fee_account;
    _config.set( config, get_self() );
}

void daoswap::setnotifiers(const name& mgr, const std::vector<name> notifiers )
{
    _verify_mgr(mgr);

    for ( const name notifier : notifiers ) {
        check( is_account( notifier ), "600406:Invalid contract account");
    }

    config_table _config( get_self(), get_self().value );
    check( _config.exists(), "600101:contract is under maintenance" );
    auto config = _config.get();    
    check(config.mgr_account == mgr,"600103:is not a valid contract manager");    

    config.notifiers = notifiers;
    _config.set( config, get_self() );
}

void daoswap::setstatus(const name& mgr,const name& status )
{
    _verify_mgr(mgr);

    config_table _config( get_self(), get_self().value );
    check( _config.exists(), "600101:contract is under maintenance" );
    auto config = _config.get();
    check(config.mgr_account == mgr,"600103:is not a valid contract manager");

    config.status = status;
    _config.set( config, get_self() );
}

void daoswap::setmintimes(const name& mgr,const uint64_t& minlocktimes)
{
    _verify_mgr(mgr);

    config_table _config( get_self(), get_self().value );
    check( _config.exists(), "600101:contract is under maintenance" );
    auto config = _config.get();
    check(config.mgr_account == mgr,"600103:is not a valid contract manager");

    config.minlocktimes = minlocktimes;
    _config.set( config, get_self() );
}


void daoswap::_verify_mgr( const name& mgr)
{
    require_auth( mgr );
    config_table _config( get_self(), get_self().value );
    check( _config.exists(), "600101:contract is under maintenance" );
    auto config = _config.get();
    check(config.mgr_account == mgr,"600103:is not a valid contract manager");
}


// Memo schemas
// ============
// Swap: `swap,<min_return>,<pair_ids>` (ex: "swap,0,SXA" )
// Deposit: `deposit,<pair_id>` (ex: "deposit,SXA")
// Withdrawal: `` (empty)
memo_schema daoswap::_parse_memo( const std::string& memo )
{
    if (memo == "") return {};

    // split memo into parts
    const std::vector<std::string> parts = utils::split(memo, ",");
    check(parts.size() <= 3, "600201:Market making or trading parameter error" );

    // memo result
    memo_schema result;
    result.action = utils::parse_name(parts[0]);
    result.min_return = 0;

    // swap action
    if ( result.action == ACTION_SWAP ) {
        result.pair_ids = _parse_memo_pair_ids( parts[2] );
        check( utils::is_digit( parts[1] ), "600201:Market making or trading parameter error" );
        result.min_return = std::stoll( parts[1] );
        check( result.min_return >= 0, "600201:Market making or trading parameter error" );
        check( result.pair_ids.size() >= 1, "600201:Market making or trading parameter error" );

    // deposit action
    } else if ( result.action == ACTION_DEPOSIT ) {
        result.pair_ids = _parse_memo_pair_ids( parts[1] );
        check( result.pair_ids.size() == 1, "600201:Market making or trading parameter error" );

    } else if ( result.action == ACTION_PLEDGE ) {
        result.min_return = std::stoll( parts[1] );
        check( parts.size() == 2, "600801:Incorrect pledge parameters" );
    } 
    return result;
}



// Memo schemas
// ============
// Single: `<pair_id>` (ex: "SXA")
// Multiple: `<pair_id>-<pair_id>` (ex: "SXA-SXB")
std::vector<symbol_code> daoswap::_parse_memo_pair_ids( const std::string& memo )
{
    pairs_table _pairs( get_self(), get_self().value );

    std::set<symbol_code> duplicates;
    std::vector<symbol_code> pair_ids;
    for ( const std::string str : utils::split(memo, "-") ) {
        const symbol_code symcode = utils::parse_symbol_code( str );
        check( symcode.raw(), "600201:Market making or trading parameter error" );
        check( _pairs.find( symcode.raw() ) != _pairs.end(), "600301:pair_id does not exist" );
        pair_ids.push_back( symcode );
        check( !duplicates.count( symcode ), "600302:invalid duplicate pair_ids" );
        duplicates.insert( symcode );
    }
    return pair_ids;
}

uint64_t daoswap::_get_amplifier( const symbol_code& pair_id)
{
   return 100;
}

double daoswap::_calculate_price( const asset& value0, const asset& value1 )
{
    const uint8_t precision_norm = max( value0.symbol.precision(), value1.symbol.precision() );
    const int128_t amount0 = utils::mul_amount128( value0.amount, precision_norm, value0.symbol.precision() );
    const int128_t amount1 = utils::mul_amount128( value1.amount, precision_norm, value1.symbol.precision() );
    return static_cast<double>(amount0) / amount1;
}

double daoswap::_calculate_virtual_price( const asset& value0, const asset& value1, const asset& supply )
{
    const uint8_t precision_norm = max( value0.symbol.precision(), value1.symbol.precision() );
    const int128_t amount0 = utils::mul_amount128( value0.amount, precision_norm, value0.symbol.precision() );
    const int128_t amount1 = utils::mul_amount128( value1.amount, precision_norm, value1.symbol.precision() );
    const int128_t amount2 = utils::mul_amount128( supply.amount, precision_norm, supply.symbol.precision() );
    return static_cast<double>( utils::add(amount0, amount1) ) / amount2;
}

