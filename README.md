# College Work - Chat Service
| Project Info      | Description |
| ----------- | ----------- |
| Class      | Intro to Operating Syetems       |
| Teammates if any   | TONGLIN CHEN        |
| Grade | A |




**The project description as follows**
# csci4061-p2
Introduction: A Chat Service
Systems programming is often associated with communication as this invariably requires coordinating multiple entities that are related only based on their desire to share information. This project focuses on developing a simple chat server and client called blather. The basic usage is in two parts.

Server
Some user starts bl-server which manages the chat "room". The server is non-interactive and will likely only print debugging output as it runs.
Client
Any user who wishes to chat runs bl-client which takes input typed on the keyboard and sends it to the server. The server broadcasts the input to all other clients who can respond by typing their own input.
Chat servers are an old idea dating to the late 1970's and if you have never used one previously, get online a bit more or read about them on Wikipedia. Even standard Unix terminals have a built-in ability to communicate with other users through commands like write, wall, and talk.

Unlike standard internet chat services, blather will be restricted to a single Unix machine and to users with permission to read related files. However, extending the programs to run via the internet will be the subject of some discussion.

Like most interesting projects, blather utilizes a combination of many different system tools. Look for the following items to arise.

Multiple communicating processes: clients to servers
Communication through FIFOs
Signal handling for graceful server shutdown
Alarm signals for periodic behavior
Input multiplexing with select()
Multiple threads in the client to handle typed input versus info from the server
Finally, this project includes some advanced features which may be implemented for extra credit.

1.1 Basic Features

The specification below is mainly concerned with basic features of the blather server and client. Implementing these can garner full credit for the assignment.

1.2 ADVANCED Features : Extra Credit

Some portions of the specification and code are marked ADVANCED to indicate optional features that can be implemented for extra credit. Some of these have dependencies so take care to read carefully.

ADVANCED features may be evaluated using tests and manual inspection criteria that are made available after the standard tests are distributed.
