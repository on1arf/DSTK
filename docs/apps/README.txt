D-STAR SWITCHING MATRIX TOOLKIT


APPLICATIONS:

As the name implies, the D-STAR Switching toolking is a TOOLKIT! It is
not a "one-application-that-does-everything". The DSTK is a collection
of tools that all do one particular job of a Switching-matrix.

Tools can do
- input (inject streams into the switching-matrix) and/or output
(extract streams from the switching-matrix)
- or do some kind of processing of streams inside the switching-matrix:
e.g. filter, convert formats, etc.



Application, i.e. programs that actually do something usefull for the
end-user, are created a bit like building a LEGO playhouse or making a
coctail-drink: by combining these individual tools, together with
external applications.

For that goal, dstk tools are designed to be able to communicate with
eachother. To do this, they use IPv6 multicast streams.


To give a better idea of this process, here is a list of applications
that can -in theory- be build on a D-STAR repeater, using tools from
the D-STAR switching-matrix toolkit, combined with some external
applications (usually unix-tools that are installed by default on
most unix-systems).


Note that not all applications listed below are currently possible
to make. The DSTK is still a project in-progress and modules are
still being developed or will be added in the future.

The goal of this list is to given an idea of the potential possibility
of the DSTK.


* Voice announcement system:

- input: local file containing audio-information in PCM format
- processing: audio conversion of PCM to AMB format
- output: local repeater RF-module


* DTMF control:

- input: capture of digital stream between RPC (repeater controller) and gateway-server
on a i-com based repeater
- processing: extract AMBE information from RPC stream
- processing: extract DTMF information from AMBE stream
- output: DTMF information to external appplication
- external application: perl-script written by the sysop of the repeater, controlling
repeater-linking based on DTMF codes



* Internet-stream of a reflector stream
- input: reflector client-software, receiving information from dplus or dextra reflector
- processing: select audio from one particular module
- processing: audio convertion of AMB to PCM format
- output: PCM stream to external application, adding silence and comfort noise when
no digital-voice stream is being received
- external application "ices": internet-streaming "source" 
- external application "icecast":  internet-streaming server


* cross-mode FM/D-STAR repeater
- input1: capture of digital stream between RPC (repeater controller) and
gateway-server on a i-com based repeater
- processing1a: extract AMBE information from RPC stream
- processing1b: audio conversion of AMBE to PCM format

- input2: "RX" of FM tranceiver, linked to "audio-in" of gateway-server,
resulting in PCM-stream
- processing2: audio-conversion of PCM to AMBE format

- output1: combined output of "processing1a" and "processing2", send to RF-module
of D-STAR repeater (no mixing, "first come, first serve" principle).
- output2: combined output of "processing1b" and "input2", send to "audio-out" of
gateway-server, connected to "TX" of FM tranceiver


For application for which all components DO ALREADY exist, check out the documentation
in docs/apps/


73
Kristoff ON1ARF
