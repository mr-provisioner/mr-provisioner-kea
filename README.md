## Dependencies

 - Kea 1.2.0
 - log4cplus (Ubuntu: liblog4cplus-dev - actually a Kea build dependency)
 - curl (Ubuntu: libcurl4-openssl-dev or libcurl4-gnutls-dev)

## Compile

### Kea Development

In the Kea source directory:

    ./configure --prefix=/opt/kea
    make -j5
    sudo make install
    
### Kea Production

It is recommended to install Kea with a lease database backend in production.
These instructions assume Ubuntu as the operating system, postgresql version 9.5
and Kea version 1.2.0. Please, refer to the relevant project documentation for
any other combination. 

Dependencies: `libpq-dev`, `postgresql-server-dev-9.5`

    ./configure --prefix=/opt/kea --with-dhcp-pgsql
    make -j5
    sudo make install   

The database will need to be set up as per Kea's upstream [instructions](https://kea.isc.org/docs/kea-guide.html#pgsql-database-create).

#### Config file

For a lease database set up, details on how to connect to it are required in the [configuration file](https://kea.isc.org/docs/kea-guide.html#dhcp4-configuration). See example:

    "Dhcp4": {
      "lease-database": {
          "type": "postgresql",
          "name": "kea_production",
          "host": "127.0.0.1",
          "user": "kea",
          "password": "kea_password"
      },
      (...)
    }

### Plugin

Assuming Kea source is in `/usr/src/kea-1.2.0` and was installed into `/opt/kea`:

    make KEA_SRC=/usr/src/kea-1.2.0 KEA_PREFIX=/opt/kea
    sudo make KEA_SRC=/usr/src/kea-1.2.0 KEA_PREFIX=/opt/kea install
