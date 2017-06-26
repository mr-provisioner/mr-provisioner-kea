## Dependencies

 - Kea 1.2.0
 - log4cplus (Ubuntu: liblog4cplus-dev - actually a Kea build dependency)
 - curl (Ubuntu: libcurl4-openssl-dev or libcurl4-gnutls-dev)

## Compile

### Kea

In the Kea source directory:

    ./configure --prefix=/opt/kea
    make -j5
    sudo make install

### Plugin

Assuming Kea source is in `/usr/src/kea-1.2.0` and was installed into `/opt/kea`:

    make KEA_SRC=/usr/src/kea-1.2.0 KEA_PREFIX=/opt/kea
    sudo make KEA_SRC=/usr/src/kea-1.2.0 KEA_PREFIX=/opt/kea install
