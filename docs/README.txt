D-STAR SWITCHING MATRIX TOOLKIT

The goal of the DSTK is as simple as ambitious: design a system able to switch any kind of 
D-STAR related stream, from any input to any output and -at the mean time- process it in any
way possible.

"Streams" can be:
- AMBE digital voice streams
- PCM or any other non-AMBE encoded digital voice streams
- slow-speed data
- digital data
- logging information
- DTMF information
- Application heartbeats, applications comments


"Input" can be:
-> AMBE streams originating on a local RF-module
-> AMBE streams originating from a remote system, coming in over the internet using the callsign routing protocol or Starnet digital
-> AMBE streams from remote repeaters or reflectors using linking protocols (dplus, dextra)
-> local files
-> analog input via the "audio in" of the server-computer
-> application generating audio streams (usually in PCM format)
-> voip-applications
-> audio-streams from non-digital repeaterlinking systems (echolink, IRLP, ...)


"Output" can be:
-> local repeater modules
-> local files
-> remote repeaters over the internet, using callsign routing protocols
-> remote repeaters over the internet, using linking protocols (dplus, dextra)
-> voip-applications
-> audio-streams to non-digital repeater linking systems (echolink, IRLP)
-> external applications taking in audio-streams (usually in PCM format)
-> external applications taking in non-audio streams (e.g. DTMF codes)


"Processing" can be:
-> transcoding audio codec formats (AMBE, PCM, ogg, speex, mp3)
-> information extracting: e.g. DTMF detection
-> selection: e.g. filter one particular module from a stream




To achieve this, the D-STAR switching-matrix toolkit uses an approach based
on "parallel programming":

Every module (input, output or processing) is a seperate program ("tool")
which executes one particular element of the switching matrix. Modules
communicate to eachother using IP multicast streams

Application are build by combining a number of "tools". External applications
(non d-star related applications) are added to achieve certain goals:
internet-streaming, local audio-playout, and others.

IP-multicast allows for "one-to-many", "many-to-one" and "many-to-many"
communication. This means that
-> one input module can server any number of processing or output modules
-> multiple inputs can be combined into one stream.


The IP multicast streams are fully documented, allowing everybody to write
extensions to the D-STAR switching-matrix.
The dstk tools are by preference open-source to maximise reuse and extensability.


This approach is designed to give operators of D-STAR systems maximum freedom to
create any application he or she wants, without any limitations impossed by the
programmers of the individual tools.

Check the docs/apps/README.txt file for more information on how to create
applications using the dstk-tools with added external tools


73
Kristoff ON1ARF


