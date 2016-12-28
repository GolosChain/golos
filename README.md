Introducing Golos (alpha)
-----------------

Seed Nodes
----------
Nodes that returns CHAIN_ID:
```
seed-node = 88.99.13.48:4243                    # @primus (DE)
seed-node = golos.imcoins.org:4243              # @dr2073 (RU)
seed-node = node.golostools.ru:4243             # @on0tole (?)
seed-node = 138.68.101.115:4243                 # @kuna (?)
seed-node = 178.62.224.148:4242                 # @vitaly-lvov (NED)
```

Other known Nodes:
```
seed-node = 136.243.33.85:4243                  # @smailer (DE)
seed-node = golos-seed.someguy123.com:4243      # @someguy123 (USA)
seed-node = steemul.ru:4243                     # @xtar (DE)
seed-node = 52.57.156.202:4243                  # @smooth (DE)
seed-node = golos.steem.ws:4243                 # @jesta (US)
seed-node = seed.roelandp.nl:4243               # @roelandp (CAN)
seed-node = golosnode.com:4243                  # @steem-id (FR)
seed-node = golos-seed.arcange.eu:4243          # @arcange (EU)
seed-node = golosnode.com:4243                  # @steem-id (FR)
seed-node = 173.208.206.34:4243                 # @serejandmyself (USA)
seed-node = 142.54.168.58:4243                  # @steemychicken1 (USA)
seed-node = 5.9.18.213:4243                     # @pfunk (DE)
seed-node = 94.23.200.113:4243                  # @steve-walschot (FR)
seed-node = 89.179.243.53:4243                  # @litrbooh (RU)
```

How to Mine
-----------

The mining algorithm used by Golos requires the owner to have access to the private key
used by the account. This means it does not favor mining pools.

    ./golosd --miner=["accountname","${WIFPRIVATEKEY}"] --witness="accountname" --seed-node="ip:port"

Make sure that your accountname is unique and not already used by someone else or your proof of work
might not be accepted by the blockchain.

OS-specific build instructions
------------------------------

See [here](https://wiki.golos.io/3-guides/ubuntu_guide.html) for Ubuntu and [here](https://wiki.golos.io/3-guides/osx_guide.html) for macOS (Apple).

cmake Build Options
--------------------------

### CMAKE_BUILD_TYPE=[Release/Debug]

Specifies whether to build with or without optimization and without or with the symbol table for debugging. Unless you are specifically
debugging or running tests, it is recommended to build as release.

### LOW_MEMORY_NODE=[OFF/ON]

Builds golosd to be a consensus only low memory node. Data and fields not needed for consensus are not stored in the object database.
This option is recommended for witnesses and seed-nodes.

### ENABLE_CONTENT_PATCHING=[ON/OFF]

Allows content to be updated using a patch rather than a complete replacement.
If you do not need an API server or need to see the result of patching content then you can set this to OFF.

### CLEAR_VOTES=[ON/OFF]

Clears old votes from memory that are not longer required for consensus.

### BUILD_GOLOS_TESTNET=[OFF/ON]

Builds golosd for use in a private testnet. Also required for correctly building unit tests
