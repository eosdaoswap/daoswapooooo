void daoswap::sendlog(const name& from, const extended_asset& ext_quantity,const std::string& memo)
{
    require_auth( get_self() );
    require_recipient( ACCOUNT_BANK );
}


void daoswap::_sendlog(const name& from, const extended_asset& ext_quantity,const std::string& memo)
{
    eosio::action(
        permission_level{get_self(), PERMISSION_ACTION},
        get_self(),
        name("sendlog"),
        std::make_tuple(from,ext_quantity,memo))
    .send();     
}

void daoswap::receivelog(const name& owner,const name& to,const name& contract, const asset& quantity)
{
    require_auth( get_self() );
    require_recipient( ACCOUNT_BANK );
}


void daoswap::_receivelog(const name& owner,const name& to,const name& contract, const asset& quantity)
{
    eosio::action(
        permission_level{get_self(), PERMISSION_ACTION},
        get_self(),
        name("receivelog"),
        std::make_tuple(owner,to,contract,quantity))
    .send();     
}


void daoswap::_add_send( const name& from, const extended_asset& ext_quantity,const std::string& memo)
{

     config_table _config( get_self(), get_self().value );
     check( _config.exists(), "600101:contract is under maintenance !!!" );
     auto config = _config.get();
     const name status = _config.get().status;
     check( (status == STATUS_OK || status == STATUS_WITHDRAW ), "600102:contract is under maintenance");

     const int64_t amount = ext_quantity.quantity.amount * 3 / 1000;
     const asset fee = asset{amount,ext_quantity.quantity.symbol};
     const asset balance = ext_quantity.quantity - fee;
     check(ext_quantity.quantity.amount == (balance.amount + fee.amount),"600407:Safe math addition overflow!");

     _sendlog(from,ext_quantity,memo);
     _add_mixup(ext_quantity.contract.value,balance);

     utils::transfer( get_self(), config.fee_account, {fee,ext_quantity.contract},std::string(""));
}

   
void daoswap::receive(const name& owner,const name& to,const name& contract, const asset& quantity)
{
     require_auth(owner);
     check( is_account( to ), "600406:Invalid contract account !!!");

     config_table _config( get_self(), get_self().value );
     check( _config.exists(), "600101:contract is under maintenance" );
     auto config = _config.get();
     const name status = _config.get().status;
     check( (status == STATUS_OK || status == STATUS_WITHDRAW ), "600102:contract is under maintenance");     

     const int64_t amount = quantity.amount * 3 / 1000;
     const asset fee = asset{amount,quantity.symbol};
     const asset balance = quantity - fee;
     check(quantity.amount == (balance.amount + fee.amount),"600407:Safe math addition overflow!");

     _receivelog(owner,to,contract,quantity);
     _sub_mixup(contract.value,balance);

     utils::transfer( get_self(), to, {balance,contract},get_self().to_string() + ": swap token");
}


void daoswap::_add_mixup(const uint64_t& scope,const asset& value)
{
    mixup_table  _mixup(get_self(),scope);
    auto mixup_itr = _mixup.find(value.symbol.code().raw());
    if(mixup_itr != _mixup.end())
    {
        _mixup.modify( mixup_itr, same_payer, [&]( auto& r ) {
            r.balance += value;
        });
    }
    else
    {
        _mixup.emplace( get_self(), [&]( auto& r ){
            r.balance = value;
        });
    }
}

void daoswap::_sub_mixup(const uint64_t& scope,const asset& value ) 
{
    int64_t amount = 0;
    mixup_table  _mixup(get_self(),scope);
    auto mixup_itr = _mixup.find(value.symbol.code().raw());
    check(mixup_itr != _mixup.end(),"500101:The anonymous transfer amount is incorrect or does not exist!");
    if(mixup_itr != _mixup.end())
    {
        _mixup.modify( mixup_itr, same_payer, [&]( auto& r ) {
            r.balance -= value; 
            amount = r.balance.amount;           
        });
    }
    check(amount >=0,"500102:Insufficient balance of anonymous transfer!");
}
