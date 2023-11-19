# UDPProtocol

This is an ongoing project I'm currently building. It is a connection-based reliable network protocol custom built on top of UDP. Intended use is for games where data is often time critical.

It offers:
- Detection of connections and disconnection
- Reliable & unreliable packets

It does NOT offer:
- Packet fragmentation
- Congestion avoidance
- Security


In the `code` is an example of how you might set up networking with my protcol alongside my core layer. 

## Build and run

1. Run `build.bat` in code folder with visual studio environment variables enabled.
2. A folder `build` has now been created with a main.exe.
3. Run the main.exe with `-host` flag to run as host
4. To runt as client, specify no flags

The server hosts on localhost by default.
