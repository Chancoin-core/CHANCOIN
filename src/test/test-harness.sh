#!/bin/bash

# Comment whatever tests you don't want guvna

./test_chancoin --color_output -m CLF -t \
arith_uint256_tests,\
addrman_tests,\
amount_tests,\
allocator_tests,\
base32_tests,\
base58_tests,\
base64_tests,\
bip32_tests,\
bloom_tests,\
bswap_tests,\
checkqueue_tests,\
coins_tests,\
compress_tests,\
crypto_tests,\
cuckoocache_tests,\
DoS_tests,\
getarg_tests,\
hash_tests,\
limitedmap_tests,\
dbwrapper_tests,\
main_tests,\
mempool_tests,\
merkle_tests,\
multisig_tests,\
net_tests,\
netbase_tests,\
pmt_tests,\
policyestimator_tests,\
PrevectorTests,\
raii_event_tests,\
random_tests,\
reverselock_tests,\
rpc_tests,\
sanity_tests,\
scheduler_tests,\
script_P2SH_tests,\
script_tests,\
scriptnum_tests,\
scrypt_tests,\
serialize_tests,\
sighash_tests,\
sigopcount_tests,\
skiplist_tests,\
streams_tests,\
timedata_tests,\
torcontrol_tests,\
transaction_tests,\
tx_validationcache_tests,\
uint256_tests,\
univalue_tests,\
util_tests,\
accounting_tests,\
wallet_tests,\
wallet_crypto

# Omitted tests
# miner_tests,\
# key_tests,
# pow_tests,\
