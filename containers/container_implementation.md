# Container Implementation Details
The following is relevant for the implementation of model containers in different languages. 
Model containers must implement functionality consistent with this information.

## Connection Lifecycle
The connection lifecycle defines the socket construction, destruction, and state-based behavior for a single Clipper session. We define a session as a sustained connection between a container and Clipper. Key points:

- Every session requires a distinct socket. When a session ends, a container must create a new socket in order to reconnect to Clipper.
- Sessions are initiated and sustained by a heartbeating process. Containers send the first heartbeat message and wait until they receive a response from Clipper. The initial detection of a heartbeat response from Clipper marks the creation of a successful connection, and the loss of a heartbeat indicates that a connection is broken.

## RPC Messages
Model containers communicate with Clipper using RPC messages of several types. Each RPC message is a [multi-part ZeroMQ message](http://zguide.zeromq.org/php:chapter2#toc11) beginning with an [empty ZeroMQ frame](http://zguide.zeromq.org/php:chapter3#The-Simple-Reply-Envelope)

### Versioning and inbound/outbound messages
Messages that a container receives from Clipper are referred to as **inbound** messages, and messages that a container sends to Clipper are referred to as **outbound** messages.

All inbound messages begin with an **RPC version tag**, represented as an unsigned, 32-bit integer. This version tag is the first part of the inbound message after the [empty delimiter frame](http://zguide.zeromq.org/php:chapter3#The-Simple-Reply-Envelope). Containers should ensure that the version tag matches the RPC version of their container and gracefully exit if a version discrepancy is detected.

### The current RPC version is: 3
    
### Message Types 
Each message contains a **Message Type** field, encoded as an **unsigned integer**, that specifies one of the following types:

* 0: *New container message*
* 1: *Container content message*  
* 2: *Heartbeat message*

The following subsections explain the function and structure of each type of message.

#### New Container Message
This type of message is sent to Clipper by a container at the beginning of a session. It carries container metadata (name, version, etc) that Clipper uses to register the container with the system. Beyond the required empty frame and **Message Type** field, *new container messages* contain the following strictly-ordered fields:

  * **Model Name**: The user-defined name of the model, as a string
  * **Model Version**: The **integer** model version. **This should be sent as a string**.
  * **Input Type**: The type of input that the model should accept. This must be one of the following **integer** values, sent as a **string**:
    * 0: Bytes 
    * 1: 32-bit Integers
    * 2: 32-bit Floats
    * 3: 64-bit Doubles
    * 4: Strings
    
The following is an example construction of a *new container message* in Python:

  ```py
    socket.send("", zmq.SNDMORE) # Sends an empty frame and indicates that more content will follow
    socket.send(struct.pack("<I", 0), zmq.SNDMORE) # Indicates that this is a new container message
    socket.send(<MODEL_NAME>, zmq.SNDMORE)
    socket.send(str(<MODEL_VERSION>), zmq.SNDMORE)
    socket.send(str(<MODEL_INPUT_TYPE>))
  ```
    
#### Container Content Messages
Once Clipper has registered a container, these content messages are exchanged between the container and Clipper in order to serve prediction requests. These messages contain serialized queries (from Clipper) or serialized responses (from the container). For more information on query-response serialization, see the "Serializing Prediction Requests" and "Serializing Prediction Responses" sections below. Beyond the required empty frame and **Message Type** field, *container content messages* contain the following strictly-ordered fields:

  * **Message Id**: A unique identifier, encoded as an unsigned integer, corresponding to the container content message. When handling a prediction request sent via a *container content message* from Clipper, the response *container content message* must specify the same **Message Id** as the request message. Clipper will use this identifier to correctly construct request-response pairs in order to return a query result.
  * **Multi-part Message Content**: A series of message parts containing byte content that represents either a serialized prediction request (in the case of inbound messages from Clipper) or a serialized prediction response (in the case of outbound messages from the container).

The following is an example construction of a *container content message* in Python:
    
 ```py
   socket.send("", zmq.SNDMORE) # Sends an empty frame and indicates that more content will follow
   socket.send(struct.pack("<I", 1), zmq.SNDMORE) # Indicates that this is a container content message
   socket.send(struct.pack("<I", <MESSAGE_ID>), zmq.SNDMORE)
   for idx in range(len(container_content)):
       serialized_content_part = container_content[idx]
       if idx == len(container_content) - 1:
           # Don't forget to remove the `SNDMORE` flag 
           # if this is the last message part
           flags = 0
       else:
           flags = zmq.SNDMORE

       socket.send(serialized_content_part, flags)
 ```
 
#### Heartbeat Messages
These messages are used for session initialization as well as maintenance. By sending and receiving heartbeats, containers are able to determine whether or not Clipper is still active and respond accordingly. Beyond the required empty frame and **Message Type** field, *heartbeat messages* contain the following strictly-ordered fields:

  * **Heartbeat Type (INBOUND ONLY)**: This field contains a binary, unsigned integer and is only present in **inbound** messages received from Clipper. The value meanings are as follows:
    * 1: This indicates that Clipper does not have any metadata for the recipient container. This serves as an indication that the container should send a *new container message* so that Clipper can register it.
    * 0: This indicates that the message is a simple heartbeat response from Clipper requiring no further action.
    
The following is an example construction of an **outbound** *heartbeat message* in Python:
 
  ```py      
    socket.send("", zmq.SNDMORE) # Sends an empty frame and indicates that more content will follow
    socket.send(struct.pack("<I", 2), zmq.SNDMORE) # Indicates that this is a heartbeat message
  ```
  
### Starting a new session
Now that we are familiar with the different types of RPC messages, we will see how a model container can use them to start a session with Clipper. The steps are as follows:

1. **Socket Creation**: Every Clipper session requires a distinct ZeroMQ socket. Create a [ZeroMQ Dealer socket](http://api.zeromq.org/3-2:zmq-socket) object. This socket can receive requests and send responses asynchronously.
  * Python example:
  
  ```py  
    import zmq
    context = zmq.Context();
    socket = context.socket(zmq.DEALER)
  ```
    
2. Once a ZeroMQ Dealer socket has been created, use it to connect to Clipper. In Python, this can be accomplished as follows:

  ```py  
    socket.connect(<CLIPPER_TCP_ADDRESS>, <CLIPPER_PORT>)
  ```

3. Then, send a *heartbeat message* to Clipper.

4. Continuously poll the socket for a *heartbeat message* response from Clipper. Because the container is connecting to Clipper via a fresh socket, this message should be of **heartbeat type** `1`. Python example:

  ```py  
   poller = zmq.Poller()
   poller.register(socket, zmq.POLLIN) # Register the socket for polling
   while(True):
       receivable_sockets = dict(poller.poll(5000)) # Poll the socket for new content for 5 seconds
       if socket in receivable_sockets and receivable_sockets[socket] == ZMQ.POLLIN:
           # The socket has a message to receive
           socket.recv()
           msg_type_bytes = socket.recv()
           msg_type = struct.unpack("<I", msg_type_bytes)[0]
           assert msg_type == MESSAGE_TYPE_HEARTBEAT
           heartbeat_type_bytes = socket.recv()
           heartbeat_type = struct.unpack("<I", heartbeat_type_bytes)[0]
           assert heartbeat_type == 1
           break
   ```
   
5. After receiving a heartbeat response from Clipper requesting metadata, the container should send back a **new container message**. This completes the initialization of a session.

### Maintaining a session
In order to maintain a session, the container should frequently request heartbeats from Clipper as well as poll its sockets for new *heartbeat* and *container content* messages using the following pattern:

1. Maintain a `last_activity` timestamp that will be used to infer whether or not Clipper is still alive. 

2. Poll the container socket for new messages for a predefined duration `D`. If a message is received before `D` elapses, process the message according to its type (*heartbeat* or *container content*) and set `last_activity` to the current time.

3. If `D` elapses, check the time elapsed since `last_activity`. If this exceeds the **socket activity timeout** `TO`, end the session. Otherwise, send a heartbeat message to Clipper.

4. Repeat steps 1-3. 

Python example:

  ```py  
   from datetime import datetime
   last_activity_time = datetime.now()
   while(True):
       receivable_sockets = dict(poller.poll(5000)) # Poll the socket for new content for 5 seconds
       if socket in receivable_sockets and receivable_sockets[socket] == ZMQ.POLLIN:
           # The socket has a message to receive
           socket.recv()
           msg_type_bytes = socket.recv()
           msg_type = struct.unpack("<I", msg_type_bytes)[0]
           # Process message according to type
           ...
           last_activity_time = datetime.now()
       else:
           time_since_activity = (datetime.now() - last_activity_time).seconds
           if time_since_activity >= 60:
               # Terminate the connection
               ...
           else:
               send_heartbeat_message(socket)
   ```

The current implementation uses the values `D = 5 seconds` and `TO = 30 seconds`. 

### Ending a session
If the container fails to receive a message from Clipper within the time limit specified by the **socket activity timeout**, the session should be terminated by executing the following steps:

1. Unregister the socket from any ZeroMQ pollers.

2. Close the socket.

Python example:

  ```py  
   poller.unregister(socket)
   socket.close()
  ```
The container should then attempt to start a new session.

### For additional container lifecycle references, see:
- [clipper/containers/python/rpc.py](https://github.com/ucbrise/clipper/blob/develop/containers/python/rpc.py)
- [clipper/containers/java/.../ModelContainer.java](https://github.com/ucbrise/clipper/blob/develop/containers/java/clipper-java-container/src/main/java/clipper/container/app/ModelContainer.java)

## Serialization Formats
RPC requests sent from Clipper to model containers are divided into two categories: **Prediction Requests** and **Feedback Requests**. Each request type has a specific serialization format that defines the container deserialization procedure. Each **request** message has a corresponding **response** message with a well-defined serialization format.

### Serializing Prediction Requests/Responses

Prediction requests are serialized in a similar fashion to prediction responses. The only difference is that prediction requests begin with an extra field.

1. **Prediction requests only:** Prediction requests begin with a request type header. The header is represented as a 32-bit unsigned integer sent as a single ZeroMQ message part. The value of this integer will be 0, indicating that the request is a prediction request.

2. This is the **first** message part in a **prediction response** and the **second** message part in a **prediction request**. This message part consists of a 64-bit unsigned integer containing the length of the prediction request metadata header (defined in field **3**).

3. The next ZeroMQ message part contains a metadata header. This is a list of 64-bit unsigned integers.
    * The metadata header begins with a 64-bit unsigned integer specifying the type of prediction data contained in the request. This unsigned integer can assume values 0-4, as defined in the **RPC Messages** section under the **New Container Message** subheader.
 
    * The next 64-bit unsigned integer in the metadata header is the number of prediction data items included in the serialized content.
 
    * The remaining values in the metadata header correspond to the size, in bytes, of each subsequent prediction data item.
 
4. The remaining message parts contain the request/response's prediction data. The number of remaining message parts is equivalent to the second element in the metadata header. The byte size of the `i`th remaining message is the `i + 2`th element of the metadata header.

### Deserializing Prediction Requests/Responses
The metadata header provides sufficient information for determining the prediction data type, the number of prediction data elements, and the size of each data element. This information can be used to receive the correct number of ZeroMQ messages and parse the byte content to obtain the appropriate data-type-specific representation. 

#### For additional deserialization references, see [clipper/containers/python/rpc.py](https://github.com/ucbrise/clipper/blob/develop/containers/python/rpc.py)
