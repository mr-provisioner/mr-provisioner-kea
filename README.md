## Dependencies

 - log4cplus (Ubuntu: liblog4cplus-dev)
 - curl (Ubuntu: libcurl4-openssl-dev or libcurl4-gnutls-dev)
 - boost (Ubuntu: libboost-dev)

## Compile

Assuming kea source is in `/usr/src/kea-1.2.0` and was installed into `/opt/kea`:

    make KEA_SRC_LIB_DIR=/usr/src/kea-1.2.0/src/lib KEA_LIB_DIR=/opt/kea/lib KEA_MSG_COMPILER=/opt/kea/bin/kea-msg-compiler
