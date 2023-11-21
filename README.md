# UDPProtocol

This is an ongoing project I'm currently building. It is a connection-based reliable network protocol custom built on top of UDP. Intended use is for games where data is often time critical.

It offers:
- Detection of connections and disconnection
- Reliable & unreliable packets

It does NOT offer:
- Packet fragmentation
- Congestion avoidance
- Security

In the `code` is an example of how you might set up networking with my protcol alongside my core layer. The example sends reliable packets back and forth between the client and host for making sure that no packet was lost.

## Build and run

Building this requires the MSVC compiler.

1. Run `build.bat` in `code` folder with `x64 Native Tools Command Prompt for VS 2022`
2. A folder `build` has now been created with a main.exe.
3. Run the main.exe with `-host` flag to run as host
4. To runt as client, specify no flags

The server hosts on localhost by default.
