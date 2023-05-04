#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>

using namespace eosio;


static constexpr name ACCOUNT_ADMIN  { name("manageoooooo") };
static constexpr name ACCOUNT_RAM  { name("eosio.ram") };
static constexpr name ACCOUNT_EOS  { name("eosio.token") };
static constexpr name ACCOUNT_LOG  { name("daologoooooo") };
static constexpr name ACCOUNT_NOTICE  { name("daonoticeooo") };
//static constexpr name ACCOUNT_PRI  { name("daoprioooooo") };
static constexpr name ACCOUNT_BANK  { name("daobankooooo") };
// static constexpr name ACCOUNT_SWAP { name("daoswapooooo") };

static constexpr name ACTION_DEPOSIT  { name("deposit") };
static constexpr name ACTION_WITHDRAW  { name("withdraw") };
static constexpr name ACTION_ORDER  { name("order") };
static constexpr name ACTION_BACK  { name("back") };
static constexpr name ACTION_CANCEL  { name("cancel") };
static constexpr name ACTION_SWAP  { name("swap") };
static constexpr name ACTION_PLEDGE  { name("pledge") };
static constexpr name ACTION_UNPLEDGE  { name("unpledge") };
static constexpr name ACTION_LOCK  { name("lock") };
static constexpr name ACTION_UNLOCK  { name("unlock") };

static constexpr name ACTION_SEND  { name("send") };// 
static constexpr name ACTION_RECEIVE  { name("receive") };

static constexpr name ACTION_CREATE  { name("create") };
static constexpr name ACTION_DELETE  { name("delete") };
static constexpr name ACTION_BANK  { name("bank") };

static constexpr name PERMISSION_ACTION  { name("active") };

static constexpr name METHOD_TOKENLOG  { name("tokenlog") };
static constexpr name METHOD_SWAPLOG  { name("swaplog") };
static constexpr name METHOD_LIQLOG  { name("liquiditylog") };
static constexpr name METHOD_PAIRIDLOG  { name("pairidlog") };
static constexpr name METHOD_PLEDGELOG  { name("pledgeslog") };

static constexpr name STATUS_OK  { name("ok") };
static constexpr name STATUS_WITHDRAW  { name("withdraw") };

static constexpr uint32_t MAX_PROTOCOL_FEE = 100;
static constexpr uint32_t MAX_TRADE_FEE = 50;
static constexpr uint32_t MAX_AMPLIFIER = 1000000;
static constexpr uint8_t MAX_PRECISION = 8;//5
static constexpr uint32_t MIN_RAMP_TIME = 86400;

static constexpr uint64_t MAX_NUMBER = 1000000000000000000;
static constexpr uint64_t MIN_NUMBER = 100000000;



struct [[eosio::table]] account {
    asset    balance;
    uint64_t primary_key()const { return balance.symbol.code().raw(); }
};
typedef eosio::multi_index< "accounts"_n, account > accounts;


struct [[eosio::table]] currency_stats {
    asset    supply;
    asset    max_supply;
    name     issuer;

    uint64_t primary_key()const { return supply.symbol.code().raw(); }
};
typedef eosio::multi_index< "stat"_n, currency_stats > stats;

/*
struct [[eosio::table]] priconfig {
    name           contract;
    symbol         sym;
    asset          min;

    uint64_t    primary_key() const { return contract.value; }
};
typedef eosio::multi_index<name("priconfig"), priconfig > priconfig_tb;   
*/

/**
 * ## STRUCT `memo_schema`
 *
 * - `{name} action` - action name ("swap", "deposit")
 * - `{vector<symbol_code>} pair_ids` - symbol codes pair ids
 * - `{int64_t} min_return` - minimum return amount expected
 *
 * ### example
 *
 * ```json
 * {
 *   "action": "swap",
 *   "pair_ids": ["AB", "BC"],
 *   "min_return": 100
 * }
 * ```
 */
struct memo_schema {
	name                         action;
	std::vector<symbol_code>     pair_ids;
	int64_t                      min_return;    
};



