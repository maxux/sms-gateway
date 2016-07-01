# sms-gateway

SMS sender/receiver with PDU format. Messages received or pending to send are saved on a sqlite3 database.

This gateway is made to be used in standalone, you can push by INSERT message into the database (they will be sent one by one).
If a message is received, it will be saved on the database, multipart are handled, temporary (segments) are saved on the database too.

For now, all raw messages are saved to be analyzed later if decoding failed.

# IMPORTANT

You must (for now) disable the PIN code of you SIM card.

# Compiling
Dependancies: 
- sqlite3 (libsqlite3-dev)

Compile: 
- `cd src && make`

# Creating empty database

Create an empty database to default location:
`cat db/schema.sql | sqlite3 db/pending.sqlite3`

# Usage

From root directory, launch the gateway. It will use `/dev/ttyUSB2` by default. Path can be specified as first argument.
`./src/sms-gateway [tty-path]`
